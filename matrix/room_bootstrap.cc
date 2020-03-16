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
	struct pkg;
	using send_join1_response = std::tuple<json::object, unique_buffer<mutable_buffer>>;

	static event::id::buf make_join(const string_view &host, const room::id &, const user::id &);
	static send_join1_response send_join(const string_view &host, const room::id &, const event::id &, const json::object &event);
	static void broadcast_join(const room &, const event &, const string_view &exclude);
	static void fetch_keys(const json::array &events);
	static void eval_auth_chain(const json::array &auth_chain);
	static void eval_state(const json::array &state);
	static void backfill(const string_view &host, const room::id &, const event::id &);
	static void worker(pkg);

	extern conf::item<seconds> make_join_timeout;
	extern conf::item<seconds> send_join_timeout;
	extern conf::item<seconds> backfill_timeout;
	extern conf::item<size_t> backfill_limit;
	extern log::log log;
}

struct ircd::m::bootstrap::pkg
{
	std::string event;
	std::string event_id;
	std::string host;
};

decltype(ircd::m::bootstrap::log)
ircd::m::bootstrap::log
{
	"m.room.bootstrap"
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

	const auto member_event_idx
	{
		m::room(room_id).get(std::nothrow, "m.room.member", user_id)
	};

	const bool existing_join
	{
		m::membership(member_event_idx, "join")
	};

	if(existing_join)
		event_id_buf = m::event_id(member_event_idx, std::nothrow);

	if(!event_id_buf)
		event_id_buf = m::bootstrap::make_join(host, room_id, user_id);

	assert(event_id_buf);

	// asynchronous; returns quickly
	room::bootstrap
	{
		event_id_buf, host
	};
}

ircd::m::room::bootstrap::bootstrap(const m::event::id &event_id,
                                    const string_view &host)
try
{
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

	m::bootstrap::pkg pkg
	{
		std::string(event.source),
		event.event_id,
		host,
	};

	context
	{
		"bootstrap",
		stack_sz,
		flags,
		std::bind(&ircd::m::bootstrap::worker, std::move(pkg))
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
		log, "Sending in %s for %s at %s to '%s'",
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
		log, "Joined to %s for %s at %s to '%s' state:%zu auth_chain:%zu",
		string_view{room_id},
		string_view{user_id},
		string_view{event_id},
		host,
		state.size(),
		auth_chain.size(),
	};

	m::bootstrap::fetch_keys(auth_chain);
	m::bootstrap::eval_auth_chain(auth_chain);

	m::bootstrap::fetch_keys(state);
	m::bootstrap::eval_state(state);

	m::bootstrap::backfill(host, room_id, event_id);

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

	// At this point we have only transmitted the join event to one bootstrap
	// server. Now that we have processed the state we know of more servers.
	// They don't know about our join event though, so we conduct a synchronous
	// broadcast to the room now manually.
	m::bootstrap::broadcast_join(room, event, host);

	log::notice
	{
		log, "Joined to %s for %s at %s reset:%zu complete",
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
		log, "Join %s with %s to %s :%s",
		json::get<"room_id"_>(event),
		string_view{event.event_id},
		string(host),
		e.what()
	};
}

//
// m::bootstrap
//

void
ircd::m::bootstrap::worker(pkg pkg)
try
{
	assert(!empty(pkg.event));
	assert(!empty(pkg.event_id));
	const m::event event
	{
		pkg.event, pkg.event_id
	};

	assert(!empty(pkg.host));
	room::bootstrap
	{
		event, pkg.host
	};
}
catch(const http::error &e)
{
	log::error
	{
		log, "(worker) Failed to bootstrap for %s to %s :%s :%s",
		pkg.event_id,
		pkg.host,
		e.what(),
		e.content,
	};
}
catch(const std::exception &e)
{
	log::error
	{
		log, "(worker) Failed to bootstrap for %s to %s :%s",
		pkg.event_id,
		pkg.host,
		e.what(),
	};
}

void
ircd::m::bootstrap::broadcast_join(const m::room &room,
                                   const m::event &event,
                                   const string_view &exclude) //TODO: XX
{
	const m::room::origins origins
	{
		room
	};

	log::info
	{
		log, "Broadcasting %s to %s estimated servers:%zu",
		string_view{event.event_id},
		string_view{room.room_id},
		origins.count(),
	};

	const json::value pdu
	{
		event.source
	};

	const vector_view<const json::value> pdus
	{
		&pdu, 1
	};

	const auto txn
	{
		m::txn::create(pdus)
	};

	char idbuf[128];
	const auto txnid
	{
		m::txn::create_id(idbuf, txn)
	};

	m::feds::opts opts;
	opts.op = feds::op::send;
	opts.exclude_myself = true;
	opts.room_id = room;
	opts.arg[0] = txnid;
	opts.arg[1] = txn;

	size_t good(0), fail(0);
	m::feds::execute(opts, [&event, &good, &fail]
	(const auto &result)
	{
		if(result.eptr)
			log::derror
			{
				log, "Failed to broadcast %s to %s :%s",
				string_view{event.event_id},
				result.origin,
				what(result.eptr),
			};

		fail += bool(result.eptr);
		good += !result.eptr;
		return true;
	});

	log::info
	{
		log, "Broadcast %s to %s good:%zu fail:%zu servers:%zu online:%zu error:%zu",
		string_view{event.event_id},
		string_view{room.room_id},
		good,
		fail,
		origins.count(),
		origins.count_online(),
		origins.count_error(),
	};
}

void
ircd::m::bootstrap::backfill(const string_view &host,
                             const m::room::id &room_id,
                             const m::event::id &event_id)
try
{
	log::info
	{
		log, "Requesting recent events for %s from %s at %s",
		string_view{room_id},
		host,
		string_view{event_id},
	};

	const unique_buffer<mutable_buffer> buf
	{
		16_KiB // headers in and out
	};

	m::fed::backfill::opts opts;
	opts.remote = host;
	opts.event_id = event_id;
	opts.limit = size_t(backfill_limit);
	m::fed::backfill request
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
		log, "Processing backfill for %s from %s at %s events:%zu",
		string_view{room_id},
		host,
		string_view{event_id},
		pdus.size(),
	};

	m::vm::opts vmopts;
	vmopts.nothrows = -1;
	vmopts.warnlog &= ~vm::fault::EXISTS;
	vmopts.fetch_state = false;
	vmopts.fetch_prev = false;
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
		log, "%s backfill @ %s from %s :%s",
		string_view{room_id},
		string_view{event_id},
		string(host),
		e.what(),
	};

	// Backfill errors are not propagated further, thus they won't stop the
	// bootstrap process. The timeline won't have any readable messages, but
	// we can remedy that later.
	//throw;
}

void
ircd::m::bootstrap::eval_state(const json::array &state)
try
{
	log::info
	{
		log, "Evaluating %zu state events...",
		state.size(),
	};

	m::vm::opts opts;
	opts.nothrows = -1;
	opts.warnlog &= ~vm::fault::EXISTS;
	opts.fetch_state = false;
	opts.fetch_prev = false;
	opts.infolog_accept = true;
	m::vm::eval
	{
		state, opts
	};
}
catch(const std::exception &e)
{
	log::error
	{
		log, "eval state :%s", e.what(),
	};

	// State errors are not propagated further, thus they won't stop the
	// bootstrap process. The room state will be incomplete, but we can
	// remedy that later.
	//throw;
}

void
ircd::m::bootstrap::eval_auth_chain(const json::array &auth_chain)
try
{
	log::info
	{
		log, "Evaluating %zu authentication events...",
		auth_chain.size(),
	};

	m::vm::opts opts;
	opts.warnlog &= ~vm::fault::EXISTS;
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
		log, "eval auth_chain :%s", e.what(),
	};

	// This needs to rethrow because any failure coming out of vm::eval to
	// process the auth_chain is a showstopper.
	throw;
}

void
ircd::m::bootstrap::fetch_keys(const json::array &events)
try
{
	std::vector<m::fed::key::server_key> queries;
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

	// All errors for the parallel key fetch are logged and then suppressed
	// here. This operation is an optimization; if there's an unexpected
	// failure here keys will just be fetched in the eval loop and bootstrap
	// will just be really slow.
	//throw;
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

	m::fed::send_join::opts opts{host};
	m::fed::send_join send_join
	{
		room_id, event_id, event, buf, std::move(opts)
	};

	send_join.wait(seconds(send_join_timeout));

	const auto send_join_code
	{
		send_join.get()
	};

	const json::array send_join_response
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

	// This needs to rethrow because if there's any error in the send_join
	// request we won't have the response data for the rest of the bootstrap
	// process.
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

	m::fed::make_join::opts opts{host};
	m::fed::make_join request
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
		response.get("room_version", "1"_sv)
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
	vmopts.auth = false;
	vmopts.user_id = user_id;
	vmopts.room_version = room_version;
	const vm::eval eval
	{
		event, content, vmopts
	};

	assert(eval.event_id);
	return eval.event_id;
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

	// This needs to rethrow because if the make_join doesn't complete we
	// won't have enough information about the room to further continue the
	// bootstrap process.
	throw;
}
