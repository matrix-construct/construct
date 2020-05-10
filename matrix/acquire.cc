// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m::acquire
{
	static bool handle_event(const room &, const event::id &, const opts &);
	static void handle_missing(const room &, const opts &);
	static void handle_room(const room &, const opts &);
	static void handle(const room::id &, const opts &);
};

decltype(ircd::m::acquire::log)
ircd::m::acquire::log
{
	"m.acquire"
};

//
// acquire::acquire
//

ircd::m::acquire::acquire::acquire(const room &room,
                                   const opts &opts)
{
	if(opts.head)
	{
		handle_room(room, opts);
		ctx::interruption_point();
	}

	if(opts.missing)
	{
		handle_missing(room, opts);
		ctx::interruption_point();
	}
}

//
// internal
//

void
ircd::m::acquire::handle_room(const room &room,
                              const opts &opts)
try
{
	const room::origins origins
	{
		room
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
	const auto &[top_event_id, top_event_depth, top_event_idx]
	{
		m::top(std::nothrow, room)
	};

	log::info
	{
		log, "Resynchronizing %s from %s [idx:%lu depth:%ld] from %zu joined servers...",
		string_view{room.room_id},
		string_view{top_event_id},
		top_event_idx,
		top_event_depth,
		origins.count(),
	};

	feds::opts fopts;
	fopts.op = feds::op::head;
	fopts.room_id = room.room_id;
	fopts.user_id = user_id;
	fopts.closure_errors = false; // exceptions wil not propagate feds::execute
	fopts.exclude_myself = true;
	const auto &top_depth(top_event_depth); // clang structured-binding & closure oops
	feds::execute(fopts, [&](const auto &result)
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

		return m::for_each(prev, [&](const event::id &event_id)
		{
			if(unlikely(ctx::interruption_requested()))
				return false;

			if(errors.count(event_id))
				return true;

			if(!m::exists(event::id(event_id)))
			{
				++fetching;
				m::acquire::opts _opts(opts);
				_opts.hint = result.origin;
				_opts.hint_only = true;
				if(!handle_event(room, event_id, _opts))
				{
					// If we fail the process the event we cache that and cease here.
					errors.emplace(event_id);
					return true;
				}
				else ++evaluated;
			}
			else ++exists;

			// If the event already exists or was successfully obtained we
			// reward the remote with gossip of events which reference this
			// event which it is unlikely to have.
			//if(gossip_enable)
			//	gossip(room_id, event_id, result.origin);

			return true;
		});
	});

	if(unlikely(ctx::interruption_requested()))
		return;

	log::info
	{
		log, "Acquired %s remote head; servers:%zu online:%zu"
		" depth:%ld lt:eq:gt %zu:%zu:%zu exist:%zu eval:%zu error:%zu",
		string_view{room.room_id},
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
		string_view{room.room_id},
		e.what(),
	};
}

void
ircd::m::acquire::handle_missing(const room &room,
                                 const opts &opts)
try
{
	const m::room::events::missing missing
	{
		room
	};

	const int64_t &room_depth
	{
		m::depth(std::nothrow, room)
	};

	const ssize_t &viewport_size
	{
		m::room::events::viewport_size
	};

	const int64_t min_depth
	{
		std::max(room_depth - viewport_size * 2, 0L)
	};

	ssize_t attempted(0);
	std::set<std::string, std::less<>> fail;
	missing.for_each(min_depth, [&]
	(const auto &event_id, const int64_t &ref_depth, const auto &ref_idx)
	{
		if(unlikely(ctx::interruption_requested()))
			return false;

		auto it{fail.lower_bound(event_id)};
		if(it == end(fail) || *it != event_id)
		{
			log::debug
			{
				log, "Fetching missing %s ref_depth:%zd in %s head_depth:%zu min_depth:%zd",
				string_view{event_id},
				ref_depth,
				string_view{room.room_id},
				room_depth,
				min_depth,
			};

			if(!handle_event(room, event_id, opts))
				fail.emplace_hint(it, event_id);
		}

		++attempted;
		return true;
	});

	if(unlikely(ctx::interruption_requested()))
		return;

	if(attempted - ssize_t(fail.size()) > 0L)
		log::info
		{
			log, "Fetched %zu recent missing events in %s attempted:%zu fail:%zu",
			attempted - fail.size(),
			string_view{room.room_id},
			attempted,
			fail.size(),
		};
}
catch(const std::exception &e)
{
	log::error
	{
		log, "Failed to synchronize missing %s :%s",
		string_view{room.room_id},
		e.what(),
	};
}

bool
ircd::m::acquire::handle_event(const room &room,
                               const event::id &event_id,
                               const opts &opts)
try
{
	fetch::opts fopts;
	fopts.op = fetch::op::event;
	fopts.room_id = room.room_id;
	fopts.event_id = event_id;
	fopts.backfill_limit = 1;
	fopts.hint = opts.hint;
	fopts.attempt_limit = opts.hint_only;
	auto future
	{
		fetch::start(fopts)
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
		response["pdus"]
	};

	const m::event event
	{
		pdus.at(0), event_id
	};

	const auto &[viewport_depth, _]
	{
		m::viewport(room.room_id)
	};

	const bool below_viewport
	{
		json::get<"depth"_>(event) < viewport_depth
	};

	if(below_viewport)
		log::debug
		{
			log, "Will not fetch children of %s depth:%ld below viewport:%ld in %s",
			string_view{event_id},
			json::get<"depth"_>(event),
			viewport_depth,
			string_view{room.room_id},
		};

	m::vm::opts vmopts;
	vmopts.infolog_accept = true;
	vmopts.fetch_prev = !below_viewport;
	vmopts.fetch_state = below_viewport;
	vmopts.warnlog &= ~vm::fault::EXISTS;
	vmopts.node_id = opts.hint;
	vmopts.notify_servers = false;
	m::vm::eval eval
	{
		event, vmopts
	};

	log::info
	{
		log, "acquired %s in %s depth:%ld viewport:%ld state:%b",
		string_view{event_id},
		string_view{room.room_id},
		json::get<"depth"_>(event),
		viewport_depth,
		defined(json::get<"state_key"_>(event)),
	};

	return true;
}
catch(const std::exception &e)
{
	log::derror
	{
		log, "Failed to acquire %s synchronizing %s :%s",
		string_view{event_id},
		string_view{room.room_id},
		e.what(),
	};

	return false;
}
