// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

/// This should be a namespace but we're stuck in struct m::init for now, so
/// this code should be portable for a future when m::init is unstructured.
struct ircd::m::init::backfill
{
	static bool handle_head(const room::id &, const event::id &, const string_view &hint);
	static void handle_missing(const room::id &);
	static void handle_room(const room::id &);
	static void worker();
	static void fini();
	static void init();

	static std::unique_ptr<context> worker_context;
	static conf::item<bool> enable;
	static conf::item<size_t> pool_size;
	static log::log log;
};

ircd::mapi::header
IRCD_MODULE
{
	"Matrix resynchronization backfilling",
	ircd::m::init::backfill::init,
	ircd::m::init::backfill::fini,
};

decltype(ircd::m::init::backfill::log)
ircd::m::init::backfill::log
{
	"m.init.backfill"
};

decltype(ircd::m::init::backfill::enable)
ircd::m::init::backfill::enable
{
	{ "name",     "m.init.backfill.enable" },
	{ "default",  true                     },
};

decltype(ircd::m::init::backfill::pool_size)
ircd::m::init::backfill::pool_size
{
	{ "name",     "m.init.backfill.pool_size" },
	{ "default",  8L                          },
};

decltype(ircd::m::init::backfill::worker_context)
ircd::m::init::backfill::worker_context;

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

	if(ircd::read_only || ircd::write_avoid)
	{
		log::warning
		{
			log, "Initial synchronization of rooms from remote servers has"
			" been disabled by the configuration to avoid write operations."
		};

		return;
	}

	assert(!worker_context);
	worker_context.reset(new context
	{
		"m.init.backfill",
		512_KiB,
		&worker,
		context::POST
	});
}

void
ircd::m::init::backfill::fini()
{
	worker_context.reset(nullptr);
}

void
ircd::m::init::backfill::worker()
try
{
	// The common case is that we're in runlevel START when this context is
	// entered; we don't want to start this operation until we're in RUN.
	run::changed::dock.wait([]
	{
		return run::level != run::level::START;
	});

	// If some other level is observed here we shouldn't run this operation.
	if(run::level != run::level::RUN)
		return;

	// Prepare to iterate all of the rooms this server is aware of which
	// contain at least one user from another server which is joined.
	rooms::opts opts;
	opts.remote_only = true;
	opts.local_joined_only = true;

	// This is only an estimate because the rooms on the server can change
	// before this task completes.
	const auto estimate
	{
		rooms::count(opts)
	};

	if(!estimate)
		return;

	log::notice
	{
		log, "Starting initial backfill of %zu rooms from other servers...",
		estimate,
	};

	// Prepare a pool of child contexts to process rooms concurrently.
	// The context pool lives directly in this frame.
	static const ctx::pool::opts pool_opts
	{
		512_KiB,               // stack sz
		size_t(pool_size),     // pool sz
	};

	ctx::pool pool
	{
		"m.init.backfill", pool_opts
	};

	ctx::dock dock;
	size_t count(0), complete(0);
	const auto each_room{[&estimate, &count, &complete, &dock]
	(const room::id &room_id)
	{
		const unwind completed{[&complete, &dock]
		{
			++complete;
			dock.notify_one();
		}};

		if(unlikely(run::level != run::level::RUN))
			return false;

		handle_room(room_id);
		handle_missing(room_id);
		log::info
		{
			log, "Initial backfill of %s complete:%zu of estimate:%zu %02.2lf%%",
			string_view{room_id},
			complete,
			estimate,
			(complete / double(estimate)) * 100.0,
			count,
		};

		return !ctx::interruption_requested();
	}};

	// Iterate the room_id's, submitting a copy of each to the next pool
	// worker; the submission blocks when all pool workers are busy, as per
	// the pool::opts.
	rooms::for_each(opts, [&pool, &each_room, &count]
	(const room::id &room_id)
	{
		++count;
		pool([&each_room, room_id(std::string(room_id))]
		{
			each_room(room_id);
		});

		return !ctx::interruption_requested();
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
	dock.wait([&complete, &count]
	{
		return complete >= count;
	});

	log::notice
	{
		log, "Initial resynchronization of %zu rooms complete.",
		count,
	};
}
catch(const ctx::interrupted &e)
{
	log::derror
	{
		log, "Worker interrupted without completing resynchronization."
	};

	throw;
}
catch(const ctx::terminated &e)
{
	log::error
	{
		log, "Worker terminated without completing resynchronization."
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
try
{
	const m::room room
	{
		room_id
	};

	const room::origins origins
	{
		room
	};

	log::debug
	{
		log, "Resynchronizing %s with %zu joined servers.",
		string_view{room_id},
		origins.count(),
	};

	// When the room isn't public we need to supply a user_id of one of our
	// users in the room to satisfy matrix protocol requirements upstack.
	const auto user_id
	{
		m::any_user(room, my_host(), "join")
	};

	size_t respond(0), behind(0), equal(0), ahead(0);
	size_t exists(0), fetching(0), evaluated(0);
	std::set<std::string, std::less<>> errors;
	const auto &[top_event_id, top_depth, top_event_idx]
	{
		m::top(std::nothrow, room)
	};

	feds::opts opts;
	opts.op = feds::op::head;
	opts.room_id = room_id;
	opts.user_id = user_id;
	opts.closure_errors = false;
	opts.exclude_myself = true;
	feds::execute(opts, [&](const auto &result)
	{
		const m::event event
		{
			result.object.get("event")
		};

		// The depth comes back as one greater than any existing
		// depth so we subtract one.
		const auto &depth
		{
			std::max(json::get<"depth"_>(event) - 1L, 0L)
		};

		++respond;
		ahead += depth > top_depth;
		equal += depth == top_depth;
		behind += depth < top_depth;
		const event::prev prev
		{
			event
		};

		m::for_each(prev, [&](const string_view &event_id)
		{
			if(errors.count(event_id))
				return true;

			if(m::exists(event::id(event_id)))
			{
				++exists;
				return true;
			}

			++fetching;
			if(!handle_head(room_id, event_id, result.origin))
			{
				errors.emplace(event_id);
				return true;
			}

			++evaluated;
			return !ctx::interruption_requested();
		});

		return !ctx::interruption_requested();
	});

	ctx::interruption_point();

	log::info
	{
		log, "acquired %s remote head; servers:%zu online:%zu"
		" depth:%ld lt:eq:gt %zu:%zu:%zu exist:%zu eval:%zu error:%zu",
		string_view{room_id},
		origins.count(),
		origins.count_online(),
		top_depth,
		behind,
		equal,
		ahead,
		exists,
		evaluated,
		errors.size(),
	};

	assert(ahead + equal + behind == respond);
}
catch(const std::exception &e)
{
	log::error
	{
		log, "Failed to synchronize recent %s :%s",
		string_view{room_id},
		e.what(),
	};
}

void
ircd::m::init::backfill::handle_missing(const room::id &room_id)
try
{
	const m::room room
	{
		room_id
	};

	const m::room::events::missing missing
	{
		room
	};

	const int64_t &room_depth
	{
		m::depth(std::nothrow, room)
	};

    const int64_t &min_depth
    {
	    std::max(room_depth - ssize_t(m::room::events::viewport_size) * 2, 0L)
    };

	log::debug
	{
		log, "Attempting to fetch recent missing events in %s depth:%ld min:%ld ...",
		string_view{room_id},
		room_depth,
		min_depth,
	};

	ssize_t attempted(0);
	std::set<std::string, std::less<>> fail;
	missing.for_each(min_depth, [&room_id, &fail, &attempted]
	(const auto &event_id, const int64_t &ref_depth, const auto &ref_idx)
	{
		auto it{fail.lower_bound(event_id)};
		if(it == end(fail) || *it != event_id)
			if(!handle_head(room_id, event_id, string_view{}))
				fail.emplace_hint(it, event_id);

		++attempted;
		return true;
	});

	if(attempted - ssize_t(fail.size()) > 0L)
		log::info
		{
			log, "Fetched %zu recent missing events in %s attempted:%zu fail:%zu",
			attempted - fail.size(),
			string_view{room_id},
			attempted,
			fail.size(),
		};
}
catch(const std::exception &e)
{
	log::error
	{
		log, "Failed to synchronize missing %s :%s",
		string_view{room_id},
		e.what(),
	};
}

bool
ircd::m::init::backfill::handle_head(const room::id &room_id,
                                     const event::id &event_id,
                                     const string_view &hint)
try
{
	fetch::opts opts;
	opts.op = fetch::op::event;
	opts.room_id = room_id;
	opts.event_id = event_id;
	opts.limit = 1;
	opts.hint = hint;
	auto future
	{
		fetch::start(opts)
	};

	m::fetch::result result
	{
		future.get()
	};

	const json::object response
	{
		result
	};

	const json::array &pdus
	{
		json::object(result).at("pdus")
	};

	const m::event event
	{
		pdus.at(0), event_id
	};

	const auto &[viewport_depth, _]
	{
		m::viewport(room_id)
	};

	const bool below_viewport
	{
		json::get<"depth"_>(event) < viewport_depth
	};

	if(below_viewport)
		log::debug
		{
			log, "skipping acquired %s head %s depth:%ld below viewport:%ld",
			string_view{room_id},
			string_view{event_id},
			json::get<"depth"_>(event),
			viewport_depth,
		};

	m::vm::opts vmopts;
	vmopts.infolog_accept = true;
	vmopts.fetch_prev = !below_viewport;
	vmopts.fetch_state = below_viewport;
	vmopts.warnlog &= ~vm::fault::EXISTS;
	vmopts.node_id = hint;
	m::vm::eval eval
	{
		event, vmopts
	};

	log::info
	{
		log, "acquired %s head %s depth:%ld viewport:%ld",
		string_view{room_id},
		string_view{event_id},
		json::get<"depth"_>(event),
		viewport_depth,
	};

	return true;
}
catch(const std::exception &e)
{
	log::derror
	{
		log, "Failed to synchronize %s with %s :%s",
		string_view{room_id},
		string_view{event_id},
		e.what(),
	};

	return false;
}
