// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

using namespace ircd;

mapi::header
IRCD_MODULE
{
	"Client 7.4.2.3 :Join"
};

resource
join_resource
{
	"/_matrix/client/r0/join/", resource::opts
	{
		"(7.4.2.3) Join room_id or alias.",
		resource::DIRECTORY,
	}
};

static void
bootstrap(const m::room::alias &,
          const m::room::id &,
          const m::user::id &);

static resource::response
_post__join(client &client,
            const resource::request &request,
            const m::room::id &room_id);

static resource::response
_post__join(client &client,
            const resource::request &request,
            const m::room::alias &room_alias);

resource::response
post__join(client &client,
           const resource::request &request)
{
	if(request.parv.size() < 1)
		throw http::error
		{
			http::MULTIPLE_CHOICES, "/join room_id or room_alias required"
		};

	char idbuf[256];
	const auto &id
	{
		url::decode(request.parv[0], idbuf)
	};

	switch(m::sigil(id))
	{
		case m::id::ROOM:
			return _post__join(client, request, m::room::id{id});

		case m::id::ROOM_ALIAS:
			return _post__join(client, request, m::room::alias{id});

		default: throw m::UNSUPPORTED
		{
			"Cannot join a room using a '%s' MXID", reflect(m::sigil(id))
		};
	}
}

resource::method
method_post
{
	join_resource, "POST", post__join,
	{
		method_post.REQUIRES_AUTH
	}
};

static resource::response
_post__join(client &client,
            const resource::request &request,
            const m::room::alias &room_alias)
{
	const m::room::id::buf room_id
	{
		m::room_id(room_alias)
	};

	if(!exists(room_id))
		bootstrap(room_alias, room_id, request.user_id);

	return _post__join(client, request, room_id);
}

/// This function forwards the join request to the /rooms/{room_id}/join
/// module so they can reuse the same code. That's done with the m::join()
/// convenience linkage.
static resource::response
_post__join(client &client,
            const resource::request &request,
            const m::room::id &room_id)
{
	m::join(room_id, request.user_id);

	return resource::response
	{
		client, json::members
		{
			{ "room_id", room_id }
		}
	};
}

static void
bootstrap(const m::room::alias &room_alias,
          const m::room::id &room_id,
          const m::user::id &user_id)
{
	const unique_buffer<mutable_buffer> buf
	{
		16_KiB
	};

	m::v1::make_join request
	{
		room_id, user_id, buf, { room_alias.host() }
	};

	request.wait(seconds(8)); //TODO: conf
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

	const auto auth_events
	{
		replace(std::string{proto.get("auth_events")}, "\\/", "/")
    };

	const auto prev_events
	{
		replace(std::string{proto.get("prev_events")}, "\\/", "/")
    };

	json::iov event;
	json::iov content;
	const json::iov::push push[]
	{
		{ event,    { "type",          "m.room.member"           }},
		{ event,    { "sender",        user_id                   }},
		{ event,    { "state_key",     user_id                   }},
		{ event,    { "membership",    "join"                    }},
		{ content,  { "membership",    "join"                    }},
		{ event,    { "prev_events",   prev_events               }},
		{ event,    { "auth_events",   auth_events               }},
		{ event,    { "prev_state",    "[]"                      }},
		{ event,    { "depth",         proto.get<long>("depth")  }},
		{ event,    { "room_id",       room_id                   }},
	};

	m::vm::opts opts;
	opts.non_conform.set(m::event::conforms::MISSING_MEMBERSHIP);
	opts.non_conform.set(m::event::conforms::MISSING_PREV_STATE);
	opts.prev_check_exists = false;
	opts.history = false;
	opts.infolog_accept = true;
	const auto event_id
	{
		m::vm::commit(event, content, opts)
	};

	const unique_buffer<mutable_buffer> ebuf
	{
		64_KiB
	};

	const m::event mevent
	{
		event_id, ebuf
	};

	const string_view strung
	{
		data(ebuf), serialized(mevent)
	};

	const unique_buffer<mutable_buffer> buf2
	{
		16_KiB
	};

	m::v1::send_join sj
	{
		room_id, event_id, strung, buf2, { room_alias.host() }
	};

	sj.wait(seconds(8)); //TODO: conf

	const auto sjcode
	{
		sj.get()
	};
}
