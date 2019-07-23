// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m::bootstrap
{
	using send_join1_response = std::tuple<json::object, unique_buffer<mutable_buffer>>;

	static event::id::buf make_join(const string_view &host, const room::id &, const user::id &);
	static send_join1_response send_join(const string_view &host, const room::id &, const event::id &, const json::object &event);
	static void fetch_keys(const json::array &events);
	static void eval_lazy_chain(const json::array &auth_chain);
	static void eval_auth_chain(const json::array &auth_chain);
	static void eval_state(const json::array &state);
	static void backfill(const string_view &host, const room::id &, const event::id &);

	extern conf::item<seconds> make_join_timeout;
	extern conf::item<seconds> send_join_timeout;
	extern conf::item<seconds> backfill_timeout;
	extern conf::item<size_t> backfill_limit;
	extern conf::item<bool> backfill_first;
	extern conf::item<bool> lazychain_enable;
	extern log::log log;
}

ircd::mapi::header
IRCD_MODULE
{
	"Matrix room bootstrap."
};

decltype(ircd::m::bootstrap::log)
ircd::m::bootstrap::log
{
	"m.bootstrap"
};

decltype(ircd::m::bootstrap::lazychain_enable)
ircd::m::bootstrap::lazychain_enable
{
	{ "name",         "ircd.client.rooms.join.lazychain.enable" },
	{ "default",      false                                     },
	{ "description",

	R"(
	During the room join bootstrap process, this controls whether the
	auth_chain in the response is only selectively processed. This is a
	safe optimization that allows the bootstrap to progress to the next
	phase. The skipped events are eventually processed during the state
	evaluation phase.
	)"}
};

decltype(ircd::m::bootstrap::backfill_first)
ircd::m::bootstrap::backfill_first
{
	{ "name",         "ircd.client.rooms.join.backfill.first" },
	{ "default",      true                                    },
	{ "description",

	R"(
	During the room join bootstrap process, this controls whether backfilling
	recent timeline events occurs before processing the room state. If true,
	user experience may be improved because their client's timeline is
	immediately populated with recent messages. Otherwise, the backfill will be
	delayed until after all state events have been processed first. Setting
	this to false is safer, as some clients may be confused by timeline events
	which are missing related state events. Note that fundamental state events
	for the room are still processed first regardless of this setting. Also
	known as the Hackfill optimization.
	)"}
};

decltype(ircd::m::bootstrap::backfill_limit)
ircd::m::bootstrap::backfill_limit
{
	{ "name",         "ircd.client.rooms.join.backfill.limit" },
	{ "default",      64L                                     },
	{ "description",

	R"(
	The number of events to request on initial backfill. Specapse may limit
	this to 50, but it also may not. Either way, a good choice is enough to
	fill a client's timeline quickly with a little headroom.
	)"}
};

decltype(ircd::m::bootstrap::backfill_timeout)
ircd::m::bootstrap::backfill_timeout
{
	{ "name",     "ircd.client.rooms.join.backfill.timeout" },
	{ "default",  15L                                       },
};

decltype(ircd::m::bootstrap::send_join_timeout)
ircd::m::bootstrap::send_join_timeout
{
	{ "name",     "ircd.client.rooms.join.send_join.timeout" },
	{ "default",  90L  /* spinappse */                       },
};

decltype(ircd::m::bootstrap::make_join_timeout)
ircd::m::bootstrap::make_join_timeout
{
	{ "name",     "ircd.client.rooms.join.make_join.timeout" },
	{ "default",  15L                                        },
};

//
// m::room::bootstrap
//

IRCD_MODULE_EXPORT
ircd::m::room::bootstrap::bootstrap(m::event::id::buf &event_id_buf,
                                    const m::room::id &room_id,
                                    const m::user::id &user_id,
                                    const string_view &host)
{
	log::info
	{
		log, "Starting in %s for %s to '%s'",
		string_view{room_id},
		string_view{user_id},
		host
	};

	// synchronous; yields ctx.
	event_id_buf = m::bootstrap::make_join(host, room_id, user_id);
	assert(event_id_buf);

	// asynchronous; returns quickly
	room::bootstrap
	{
		event_id_buf, host
	};
}

IRCD_MODULE_EXPORT
ircd::m::room::bootstrap::bootstrap(const m::event::id &event_id,
                                    const string_view &host)
try
{
	struct pkg
	{
		std::string event;
		std::string event_id;
		std::string host;
	};

	static const context::flags flags
	{
		context::POST | context::DETACH
	};

	static const auto stack_sz
	{
		128_KiB
	};

	const m::event::fetch event
	{
		event_id
	};
	assert(event.valid);
	assert(event.source);

	context
	{
		"bootstrap",
		stack_sz,
		flags,
		[p(pkg{std::string(event.source), event.event_id, host})]
		{
			assert(!empty(p.event));
			assert(!empty(p.event_id));
			const m::event event
			{
				p.event, p.event_id
			};

			assert(!empty(p.host));
			room::bootstrap
			{
				event, p.host
			};
		}
	};
}
catch(const std::exception &e)
{
	log::error
	{
		log, "Failed to bootstrap for %s to %s :%s",
		string_view{event_id},
		host,
		e.what(),
	};
}

IRCD_MODULE_EXPORT
ircd::m::room::bootstrap::bootstrap(const m::event &event,
                                    const string_view &host)
try
{
	const m::event::id &event_id
	{
		event.event_id
	};

	const m::room::id &room_id
	{
		at<"room_id"_>(event)
	};

	const m::user::id &user_id
	{
		at<"sender"_>(event)
	};

	const m::room room
	{
		room_id, event_id
	};

	log::info
	{
		log, "join bootstrap sending in %s for %s at %s to '%s'",
		string_view{room_id},
		string_view{user_id},
		string_view{event_id},
		host
	};

	assert(event.source);
	const auto &[response, buf]
	{
		m::bootstrap::send_join(host, room_id, event_id, event.source)
	};

	const json::array &auth_chain
	{
		response["auth_chain"]
	};

	const json::array &state
	{
		response["state"]
	};

	log::info
	{
		log, "join bootstrap joined to %s for %s at %s to '%s' state:%zu auth_chain:%zu",
		string_view{room_id},
		string_view{user_id},
		string_view{event_id},
		host,
		state.size(),
		auth_chain.size(),
	};

	if(m::bootstrap::lazychain_enable)
		m::bootstrap::eval_lazy_chain(auth_chain);
	else
		m::bootstrap::eval_auth_chain(auth_chain);

	if(m::bootstrap::backfill_first)
	{
		m::bootstrap::backfill(host, room_id, event_id);
		m::bootstrap::eval_state(state);
	} else {
		m::bootstrap::eval_state(state);
		m::bootstrap::backfill(host, room_id, event_id);
	}

	// After we just received and processed all of this state with only a
	// recent backfill our system doesn't know if state events which are
	// unreferenced are simply referenced by events we just don't have. They
	// will all be added to the room::head and each future event we transmit
	// to the room will drain that list little by little. But the cost of all
	// these references is too high. We take the easy route here and simply
	// clear the head of every event except our own join event.
	const size_t num_reset
	{
		m::room::head::reset(room)
	};

	log::info
	{
		log, "join bootstrap joined to %s for %s at %s reset:%zu complete",
		string_view{room_id},
		string_view{user_id},
		string_view{event_id},
		num_reset,
	};
}
catch(const std::exception &e)
{
	log::error
	{
		log, "join bootstrap for %s to %s :%s",
		string_view{event.event_id},
		string(host),
		e.what()
	};
}

void
ircd::m::bootstrap::backfill(const string_view &host,
                             const m::room::id &room_id,
                             const m::event::id &event_id)
try
{
	const unique_buffer<mutable_buffer> buf
	{
		16_KiB // headers in and out
	};

	m::v1::backfill::opts opts{host};
	opts.dynamic = true;
	opts.event_id = event_id;
	opts.limit = size_t(backfill_limit);
	m::v1::backfill request
	{
		room_id, buf, std::move(opts)
	};

	request.wait(seconds(backfill_timeout));

	const auto code
	{
		request.get()
	};

	const json::object &response
	{
		request.in.content
	};

	const json::array &pdus
	{
		response["pdus"]
	};

	log::info
	{
		log, "join bootstrap processing backfill for %s from %s at %s events:%zu",
		string_view{room_id},
		host,
		string_view{event_id},
		pdus.size(),
	};

	m::vm::opts vmopts;
	vmopts.nothrows = -1;
	vmopts.fetch_state_check = false;
	vmopts.fetch_prev_check = false;
	vmopts.infolog_accept = false;
	m::vm::eval
	{
		pdus, vmopts
	};
}
catch(const std::exception &e)
{
	log::error
	{
		log, "join bootstrap %s backfill @ %s from %s :%s",
		string_view{room_id},
		string_view{event_id},
		string(host),
		e.what(),
	};
}

void
ircd::m::bootstrap::eval_state(const json::array &state)
try
{
	m::vm::opts opts;
	opts.nothrows = -1;
	opts.fetch_prev_check = false;
	opts.fetch_state_check = false;
	opts.infolog_accept = false;
	m::vm::eval
	{
		state, opts
	};
}
catch(const std::exception &e)
{
	log::error
	{
		log, "join bootstrap eval state :%s", e.what(),
	};
}

void
ircd::m::bootstrap::eval_auth_chain(const json::array &auth_chain)
try
{
	fetch_keys(auth_chain);

	m::vm::opts opts;
	opts.infolog_accept = true;
	opts.fetch = false;
	m::vm::eval
	{
		auth_chain, opts
	};
}
catch(const std::exception &e)
{
	log::error
	{
		log, "join bootstrap eval auth_chain :%s", e.what(),
	};

	throw;
}

void
ircd::m::bootstrap::eval_lazy_chain(const json::array &auth_chain)
{
	m::vm::opts opts;
	opts.infolog_accept = true;
	opts.fetch = false;

	// Parse and sort the auth_chain first so we don't have to keep scanning
	// the JSON to do the various operations that follow.
	std::vector<m::event> events(begin(auth_chain), end(auth_chain));
	std::sort(begin(events), end(events));

	// When we selectively evaluate the auth_chain below we may need to feed
	// the vm certain member events first to avoid complications; this
	// subroutine will find them.
	const auto find_member{[&events]
	(const m::user::id &user_id, const int64_t &depth)
	{
		const auto it(std::find_if(rbegin(events), rend(events), [&user_id, &depth]
		(const m::event &event)
		{
			return json::get<"depth"_>(event) < depth &&
			       json::get<"type"_>(event) == "m.room.member" &&
			       json::get<"state_key"_>(event) == user_id;
		}));

		if(unlikely(it == rend(events)))
			throw m::NOT_FOUND
			{
				"No m.room.member event for %s found in auth chain.",
				string_view{user_id}
			};

		return *it;
	}};

	for(const auto &event : events)
	{
		// Skip all events which aren't power events. We don't need them
		// here yet. They can wait until state evaluation later.
		if(!m::event::auth::is_power_event(event))
			continue;

		// Find the member event for the sender of this power event so the
		// system is aware of their identity first; this isn't done for the
		// create event because the vm expects that first regardless.
		if(json::get<"type"_>(event) != "m.room.create")
		{
			const auto &member_event
			{
				find_member(at<"sender"_>(event), at<"depth"_>(event))
			};

			m::vm::eval
			{
				member_event, opts
			};
		}

		m::vm::eval
		{
			event, opts
		};
	}
}

void
ircd::m::bootstrap::fetch_keys(const json::array &events)
try
{
	std::vector<m::v1::key::server_key> queries;
	queries.reserve(events.size());

	for(const json::object &event : events)
		for(const auto &[server_name, signatures] : json::object(event["signatures"]))
			for(const auto &[key_id, signature] : json::object(signatures))
				queries.emplace_back(unquote(event.at("origin")), key_id);

	std::sort(begin(queries), end(queries));
	queries.erase(std::unique(begin(queries), end(queries)), end(queries));

	log::info
	{
		log, "Fetching %zu keys for %zu events...",
		queries.size(),
		events.size(),
	};

	const size_t fetched
	{
		m::keys::fetch(queries)
	};

	log::info
	{
		log, "Fetched %zu of %zu keys for %zu events",
		fetched,
		queries.size(),
		events.size(),
	};
}
catch(const std::exception &e)
{
	log::error
	{
		log, "Error when fetching keys for %zu events :%s",
		events.size(),
	};
}

ircd::m::bootstrap::send_join1_response
ircd::m::bootstrap::send_join(const string_view &host,
                              const m::room::id &room_id,
                              const m::event::id &event_id,
                              const json::object &event)
try
{
	const unique_buffer<mutable_buffer> buf
	{
		16_KiB // headers in and out
	};

	m::v1::send_join::opts opts{host};
	opts.dynamic = true;
	m::v1::send_join send_join
	{
		room_id, event_id, event, buf, std::move(opts)
	};

	send_join.wait(seconds(send_join_timeout));

	const auto send_join_code
	{
		send_join.get()
	};

	const json::array &send_join_response
	{
		send_join
	};

	const uint more_send_join_code
	{
		send_join_response.at<uint>(0)
	};

	const json::object &send_join_response_data
	{
		send_join_response[1]
	};

	assert(!!send_join.in.dynamic);
	return
	{
		send_join_response_data,
		std::move(send_join.in.dynamic)
	};
}
catch(const std::exception &e)
{
	log::error
	{
		log, "Bootstrap %s @ %s send_join to %s :%s",
		string_view{room_id},
		string_view{event_id},
		string(host),
		e.what(),
	};

	throw;
}

ircd::m::event::id::buf
ircd::m::bootstrap::make_join(const string_view &host,
                              const m::room::id &room_id,
                              const m::user::id &user_id)
try
{
	const unique_buffer<mutable_buffer> buf
	{
		16_KiB // headers in and out
	};

	m::v1::make_join::opts opts{host};
	m::v1::make_join request
	{
		room_id, user_id, buf, std::move(opts)
	};

	request.wait(seconds(make_join_timeout));
	const auto code
	{
		request.get()
	};

	const json::object &response
	{
		request.in.content
	};

	const json::string &room_version
	{
		response.get("room_version", "1")
	};

	const json::object &proto
	{
		response.at("event")
	};

	const json::array &auth_events
	{
		proto.get("auth_events")
    };

	const json::array &prev_events
	{
		proto.get("prev_events")
    };

	json::iov event;
	json::iov content;
	const json::iov::push push[]
	{
		{ event,    { "type",          "m.room.member"           }},
		{ event,    { "sender",        user_id                   }},
		{ event,    { "state_key",     user_id                   }},
		{ content,  { "membership",    "join"                    }},
		{ event,    { "prev_events",   prev_events               }},
		{ event,    { "auth_events",   auth_events               }},
		{ event,    { "prev_state",    "[]"                      }},
		{ event,    { "depth",         proto.get<long>("depth")  }},
		{ event,    { "room_id",       room_id                   }},
	};

	const m::user user{user_id};
	const m::user::profile profile{user};

	char displayname_buf[256];
	const string_view displayname
	{
		profile.get(displayname_buf, "displayname")
	};

	char avatar_url_buf[256];
	const string_view avatar_url
	{
		profile.get(avatar_url_buf, "avatar_url")
	};

	const json::iov::add _displayname
	{
		content, !empty(displayname),
		{
			"displayname", [&displayname]() -> json::value
			{
				return displayname;
			}
		}
	};

	const json::iov::add _avatar_url
	{
		content, !empty(avatar_url),
		{
			"avatar_url", [&avatar_url]() -> json::value
			{
				return avatar_url;
			}
		}
	};

	m::vm::copts vmopts;
	vmopts.infolog_accept = true;
	vmopts.fetch = false;
	vmopts.eval = false;
	vmopts.user_id = user_id;
	vmopts.room_version = room_version;
	vm::eval eval
	{
		event, content, vmopts
	};

	const m::event::id::buf &ret
	{
		eval
	};

	if(unlikely(!ret))
		throw m::UNAVAILABLE
		{
			"Unknown error"
		};

	return ret;
}
catch(const std::exception &e)
{
	log::error
	{
		log, "Bootstrap %s for %s make_join to %s :%s",
		string_view{room_id},
		string_view{user_id},
		string(host),
		e.what(),
	};

	throw;
}
