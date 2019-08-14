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

void
ircd::m::init::backfill::init()
{
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
	m::rooms::opts opts;
	opts.remote_joined_only = true;
	m::rooms::for_each(opts, []
	(const m::room::id &room_id)
	{
		handle_room(room_id);
		return true;
	});
}
catch(const ctx::interrupted &e)
{
	log::derror
	{
		log, "Worker interrupted :%s",
		e.what(),
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

	const m::room::origins origins
	{
		room
	};

	log::debug
	{
		log, "Resynchronizing %s with %zu joined servers.",
		string_view{room_id},
		origins.count(),
	};

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
