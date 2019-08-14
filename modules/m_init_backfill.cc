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
	static void handle_room(const room::id &);
	static void worker();
	static void fini();
	static void init();

	static conf::item<bool> enable;
	static std::unique_ptr<context> worker_context;
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

decltype(ircd::m::init::backfill::worker_context)
ircd::m::init::backfill::worker_context;

decltype(ircd::m::init::backfill::enable)
ircd::m::init::backfill::enable
{
	{ "name",     "m.init.backfill.enable" },
	{ "default",  true                     },
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

	log::debug
	{
		log, "Starting initial resynchronization from other servers..."
	};

	// Iterate all of the rooms this server is aware of which contain
	// at least one user from another server which is joined to the room.
	rooms::opts opts;
	opts.remote_joined_only = true;

	size_t count(0);
	rooms::for_each(opts, [&count]
	(const room::id &room_id)
	{
		handle_room(room_id);
		++count;
		return true;
	});

	log::info
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

	size_t respond(0), behind(0), equal(0), ahead(0), exists(0), fetching(0);
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

		ahead += depth > top_depth;
		equal += depth == top_depth;
		behind += depth < top_depth;
		respond++;

		const event::prev prev{event};
		m::for_each(prev, [&](const event::id &event_id)
		{
			if(m::exists(event_id))
			{
				++exists;
				return true;
			}

			fetching += fetch::start(room_id, event_id);
			return true;
		});

		return true;
	});

	log::info
	{
		log, "acquired %s remote head; depth:%ld servers:%zu online:%zu error:%zu"
		" respond:%zu behind:%zu equal:%zu ahead:%zu fetching:%zu",
		string_view{room_id},
		top_depth,
		origins.count(),
		origins.count_online(),
		origins.count_error(),
		respond,
		behind,
		equal,
		ahead,
		fetching,
	};

	assert(ahead + equal + behind == respond);
}
catch(const std::exception &e)
{
	log::error
	{
		log, "Failed to synchronize %s :%s",
		string_view{room_id},
		e.what(),
	};
}
