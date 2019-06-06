// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include "rooms.h"

using namespace ircd::m;
using namespace ircd;

static event::id::buf
bootstrap(const net::hostport &host,
          const m::room::id &room_id,
          const m::user::id &user_id);

resource::response
post__join(client &client,
           const resource::request &request,
           const room::id &room_id)
{
	const string_view &third_party_signed
	{
		unquote(request["third_party_signed"])
	};

	const string_view &server_name
	{
		unquote(request["server_name"])
	};

	const m::room room
	{
		room_id
	};

	m::join(room, request.user_id);

	return resource::response
	{
		client, json::members
		{
			{ "room_id", room_id }
		}
	};
}

event::id::buf
IRCD_MODULE_EXPORT
ircd::m::join(const room &room,
              const id::user &user_id)
{
	if(!exists(room))
	{
		const auto &room_id(room.room_id);
		return bootstrap(room_id.host(), room_id, user_id); //TODO: host
	}

	json::iov event;
	json::iov content;
	const json::iov::push push[]
	{
		{ event,    { "type",        "m.room.member"  }},
		{ event,    { "sender",      user_id          }},
		{ event,    { "state_key",   user_id          }},
		{ content,  { "membership",  "join"           }},
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

	return commit(room, event, content);
}

event::id::buf
IRCD_MODULE_EXPORT
ircd::m::join(const m::room::alias &room_alias,
              const m::user::id &user_id)
{
	const room::id::buf room_id
	{
		m::room_id(room_alias)
	};

	if(!exists(room_id))
		return bootstrap(room_alias.host(), room_id, user_id);

	const m::room room
	{
		room_id
	};

	return m::join(room, user_id);
}

//
// bootstrap
//

static event::id::buf
bootstrap_make_join(const net::hostport &host,
                    const m::room::id &,
                    const m::user::id &);

static std::tuple<json::object, unique_buffer<mutable_buffer>>
bootstrap_send_join(const net::hostport &host,
                    const m::room::id &,
                    const m::event::id &);

static void
bootstrap_eval_auth_chain(const json::array &state);

static void
bootstrap_eval_state(const json::array &state);

static void
bootstrap_backfill(const net::hostport &host,
                   const m::room::id &,
                   const m::event::id &);

conf::item<seconds>
make_join_timeout
{
	{ "name",     "ircd.client.rooms.join.make_join.timeout" },
	{ "default",  15L                                        },
};

conf::item<seconds>
send_join_timeout
{
	{ "name",     "ircd.client.rooms.join.send_join.timeout" },
	{ "default",  45L  /* spinappse */                       },
};

conf::item<seconds>
backfill_timeout
{
	{ "name",     "ircd.client.rooms.join.backfill.timeout" },
	{ "default",  15L                                       },
};

conf::item<size_t>
backfill_limit
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

event::id::buf
bootstrap(const net::hostport &host,
          const m::room::id &room_id,
          const m::user::id &user_id)
{
	thread_local char rembuf[512];
	log::debug
	{
		m::log, "join bootstrap starting in %s for %s to '%s'",
		string_view{room_id},
		string_view{user_id},
		string(rembuf, host),
	};

	const auto event_id
	{
		bootstrap_make_join(host, room_id, user_id)
	};

	log::info
	{
		m::log, "join bootstrap sending in %s for %s at %s to '%s'",
		string_view{room_id},
		string_view{user_id},
		string_view{event_id},
		string(rembuf, host),
	};

	const auto &[response, buf]
	{
		bootstrap_send_join(host, room_id, event_id)
	};

	const json::string &origin
	{
		response["origin"]
	};

	log::info
	{
		m::log, "join bootstrap joined to %s for %s at %s to '%s' reporting '%s'",
		string_view{room_id},
		string_view{user_id},
		string_view{event_id},
		string(rembuf, host),
		string_view{origin},
	};

	bootstrap_eval_auth_chain(response["auth_chain"]);
	bootstrap_eval_state(response["state"]);
	bootstrap_backfill(host, room_id, event_id);

	log::info
	{
		m::log, "join bootstrap joined to %s for %s at %s complete",
		string_view{room_id},
		string_view{user_id},
		string_view{event_id},
	};

	return event_id;
}

void
bootstrap_backfill(const net::hostport &host,
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

	// eval
	{
		m::vm::opts opts;
		opts.fetch_state_check = false;
		m::vm::eval
		{
			pdus, opts
		};
	}
}
catch(const std::exception &e)
{
	log::error
	{
		m::log, "Bootstrap %s backfill @ %s from %s :%s",
		string_view{room_id},
		string_view{event_id},
		string(host),
		e.what(),
	};
}

void
bootstrap_eval_state(const json::array &state)
{
	m::vm::opts opts;
	opts.fetch_prev_check = false;
	opts.fetch_state_check = false;
	m::vm::eval
	{
		state, opts
	};
}

void
bootstrap_eval_auth_chain(const json::array &auth_chain)
{
	m::vm::opts opts;
	opts.infolog_accept = true;
	opts.fetch = false;
	m::vm::eval
	{
		auth_chain, opts
	};
}

std::tuple<json::object, unique_buffer<mutable_buffer>>
bootstrap_send_join(const net::hostport &host,
                    const m::room::id &room_id,
                    const m::event::id &event_id)
try
{
	const m::event::fetch event
	{
		event_id
	};

	if(unlikely(!event.source))
		throw panic
		{
			"Missing event.source JSON in event::fetch"
		};

	const unique_buffer<mutable_buffer> buf
	{
		16_KiB // headers in and out
	};

	m::v1::send_join::opts opts{host};
	opts.dynamic = true;
	m::v1::send_join send_join
	{
		room_id, event_id, event.source, buf, std::move(opts)
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
		m::log, "Bootstrap %s @ %s send_join to %s :%s",
		string_view{room_id},
		string_view{event_id},
		string(host),
		e.what(),
	};

	throw;
}

event::id::buf
bootstrap_make_join(const net::hostport &host,
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

	// eval
	{
		m::vm::copts opts;
		opts.infolog_accept = true;
		opts.fetch_auth_check = false;
		opts.fetch_state_check = false;
		opts.fetch_prev_check = false;
		opts.eval = false;
		const m::event::id::buf event_id
		{
			m::vm::eval
			{
				event, content, opts
			}
		};

		return event_id;
	}
}
catch(const std::exception &e)
{
	log::error
	{
		m::log, "Bootstrap %s for %s make_join to %s :%s",
		string_view{room_id},
		string_view{user_id},
		string(host),
		e.what(),
	};

	throw;
}
