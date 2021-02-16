// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m::init::backfill
{
	void handle_room(const room::id &);
	void worker();

	extern size_t count, complete;
	extern ircd::run::changed handle_quit;
	extern ctx::ctx *worker_context;
	extern ctx::pool *worker_pool;
	extern conf::item<seconds> delay;
	extern conf::item<seconds> gossip_timeout;
	extern conf::item<bool> gossip_enable;
	extern conf::item<bool> local_joined_only;
	extern conf::item<bool> reset_head;
	extern conf::item<bool> reset_state;
	extern conf::item<bool> reset_state_space;
	extern conf::item<size_t> viewports;
	extern conf::item<size_t> attempt_max;
	extern conf::item<size_t> pool_size;
	extern conf::item<bool> enable;
	extern log::log log;
};

decltype(ircd::m::init::backfill::log)
ircd::m::init::backfill::log
{
	"m.init.backfill"
};

decltype(ircd::m::init::backfill::enable)
ircd::m::init::backfill::enable
{
	{ "name",     "ircd.m.init.backfill.enable" },
	{ "default",  true                          },
};

decltype(ircd::m::init::backfill::pool_size)
ircd::m::init::backfill::pool_size
{
	{ "name",     "ircd.m.init.backfill.pool_size" },
	{ "default",  32L                              },
};

decltype(ircd::m::init::backfill::local_joined_only)
ircd::m::init::backfill::local_joined_only
{
	{ "name",     "ircd.m.init.backfill.local_joined_only" },
	{ "default",  true                                     },
};

decltype(ircd::m::init::backfill::reset_head)
ircd::m::init::backfill::reset_head
{
	{ "name",     "ircd.m.init.backfill.reset.head" },
	{ "default",  true                              },
};

decltype(ircd::m::init::backfill::reset_state)
ircd::m::init::backfill::reset_state
{
	{ "name",     "ircd.m.init.backfill.reset.state" },
	{ "default",  true                               },
};

decltype(ircd::m::init::backfill::reset_state_space)
ircd::m::init::backfill::reset_state_space
{
	{ "name",     "ircd.m.init.backfill.reset.state_space" },
	{ "default",  false                                    },
};

decltype(ircd::m::init::backfill::gossip_enable)
ircd::m::init::backfill::gossip_enable
{
	{ "name",     "ircd.m.init.backfill.gossip.enable" },
	{ "default",  true                                 },
};

decltype(ircd::m::init::backfill::gossip_timeout)
ircd::m::init::backfill::gossip_timeout
{
	{ "name",     "ircd.m.init.backfill.gossip.timeout" },
	{ "default",  5L                                    },
};

decltype(ircd::m::init::backfill::delay)
ircd::m::init::backfill::delay
{
	{ "name",     "ircd.m.init.backfill.delay"  },
	{ "default",  15L                           },
};

decltype(ircd::m::init::backfill::viewports)
ircd::m::init::backfill::viewports
{
	{ "name",     "ircd.m.init.backfill.viewports"  },
	{ "default",  4L                                },
};

decltype(ircd::m::init::backfill::attempt_max)
ircd::m::init::backfill::attempt_max
{
	{ "name",     "ircd.m.init.backfill.attempt_max"  },
	{ "default",  8L                                  },
};

decltype(ircd::m::init::backfill::count)
ircd::m::init::backfill::count;

decltype(ircd::m::init::backfill::complete)
ircd::m::init::backfill::complete;

decltype(ircd::m::init::backfill::worker_pool)
ircd::m::init::backfill::worker_pool;

decltype(ircd::m::init::backfill::worker_context)
ircd::m::init::backfill::worker_context;

decltype(ircd::m::init::backfill::handle_quit)
ircd::m::init::backfill::handle_quit
{
	run::level::QUIT, []
	{
		fini();
	}
};

void
ircd::m::init::backfill::init()
{
	if(!enable)
	{
		log::warning
		{
			log, "Initial synchronization of rooms from remote servers has"
			" been disabled by the configuration. Not fetching latest events."
		};

		return;
	}

	ctx::context context
	{
		"m.init.backfill",
		512_KiB,
		&worker,
		context::POST
	};

	// Detach into the worker_context ptr. When the worker finishes it
	// will free itself.
	assert(!worker_context);
	worker_context = context.detach();
}

void
ircd::m::init::backfill::fini()
noexcept
{
	if(worker_context)
		log::debug
		{
			log, "Terminating worker context..."
		};

	if(worker_pool)
		worker_pool->terminate();

	if(worker_context)
		ctx::terminate(*worker_context);

	worker_context = nullptr;
	worker_pool = nullptr;
}

void
ircd::m::init::backfill::worker()
try
{
	// Wait for runlevel RUN before proceeding...
	run::barrier<ctx::interrupted>{};

	// Set a low priority for this context; see related pool_opts
	ionice(ctx::cur(), 4);
	nice(ctx::cur(), 4);

	// Prepare to iterate all of the rooms this server is aware of which
	// contain at least one member from another server in any state, and
	// one member from our server in a joined state.
	rooms::opts opts;
	opts.remote_only = true;
	opts.local_joined_only = local_joined_only;

	// This is only an estimate because the rooms on the server can change
	// before this task completes.
	const auto estimate
	{
		1UL //rooms::count(opts)
	};

	if(!estimate)
		return;

	// Wait a delay before starting.
	ctx::sleep(seconds(delay));

	log::notice
	{
		log, "Starting initial backfill of rooms from other servers...",
		estimate,
	};

	// Prepare a pool of child contexts to process rooms concurrently.
	// The context pool lives directly in this frame.
	static const ctx::pool::opts pool_opts
	{
		512_KiB,               // stack sz
		size_t(pool_size),     // pool sz
		-1,                    // queue max hard
		0,                     // queue max soft
		true,                  // queue max blocking
		true,                  // queue max warning
		3,                     // ionice
		3,                     // nice
	};

	ctx::pool pool
	{
		"m.init.backfill", pool_opts
	};

	const scope_restore backfill_worker_pool
	{
		backfill::worker_pool, std::addressof(pool)
	};

	// Iterate the room_id's, submitting a copy of each to the next pool
	// worker; the submission blocks when all pool workers are busy, as per
	// the pool::opts.
	ctx::dock dock;
	const ctx::uninterruptible ui;
	rooms::for_each(opts, [&pool, &estimate, &dock]
	(const room::id &room_id)
	{
		if(unlikely(ctx::interruption_requested()))
			return false;

		++count;
		pool([&, room_id(std::string(room_id))] // asynchronous
		{
			const unwind completed{[&dock]
			{
				++complete;
				dock.notify_one();
			}};

			handle_room(room_id);

			log::info
			{
				log, "Initial backfill of %s complete:%zu", //estimate:%zu %02.2lf%%",
				string_view{room_id},
				complete,
				estimate,
				(complete / double(estimate)) * 100.0,
			};
		});

		return true;
	});

	if(complete < count)
		log::dwarning
		{
			log, "Waiting for initial resynchronization count:%zu complete:%zu rooms...",
			count,
			complete,
		};

	// All rooms have been submitted to the pool but the pool workers might
	// still be busy. If we unwind now the pool's dtor will kill the workers
	// so we synchronize their completion here.
	dock.wait([]
	{
		return complete >= count;
	});

	if(count)
		log::notice
		{
			log, "Initial resynchronization of %zu rooms completed.",
			count,
		};
}
catch(const ctx::interrupted &e)
{
	if(count)
		log::derror
		{
			log, "Worker interrupted without completing resynchronization of all rooms."
		};

	throw;
}
catch(const ctx::terminated &e)
{
	if(count)
		log::error
		{
			log, "Worker terminated without completing resynchronization of all rooms."
		};

	throw;
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "Worker fatal :%s",
		e.what(),
	};
}

void
ircd::m::init::backfill::handle_room(const room::id &room_id)
{
	{
		struct m::acquire::opts opts;
		opts.room = room_id;
		opts.viewport_size = ssize_t(m::room::events::viewport_size);
		opts.viewport_size *= size_t(viewports);
		opts.vmopts.infolog_accept = true;
		opts.vmopts.warnlog &= ~vm::fault::EXISTS;
		opts.attempt_max = size_t(attempt_max);
		m::acquire
		{
			opts
		};
	}

	if(reset_head)
		room::head::reset(room(room_id));

	if(reset_state && reset_state_space)
		room::state::space::rebuild
		{
			room_id
		};

	if(reset_state)
		room::state::rebuild
		{
			room_id
		};

	if((false))
	{
		struct m::gossip::opts opts;
		opts.room = room_id;
		m::gossip
		{
			opts
		};
	}
}
