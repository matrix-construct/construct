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
bootstrap(const m::room::alias &room_alias,
          const m::room::id &room_id,
          const m::user::id &user_id);

extern "C" event::id::buf
join__room_user(const room &room,
                const id::user &user_id);

extern "C" event::id::buf
join__alias_user(const m::room::alias &room_alias,
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

	join__room_user(room_id, request.user_id);

	return resource::response
	{
		client, json::members
		{
			{ "room_id", room_id }
		}
	};
}

event::id::buf
join__room_user(const room &room,
                const id::user &user_id)
{
	if(!exists(room))
		throw m::NOT_FOUND
		{
			"Not aware of room %s",
			string_view{room.room_id}
		};

	json::iov event;
	json::iov content;
	const json::iov::push push[]
	{
		{ event,    { "type",        "m.room.member"  }},
		{ event,    { "sender",      user_id          }},
		{ event,    { "state_key",   user_id          }},
		{ event,    { "membership",  "join"           }},
		{ content,  { "membership",  "join"           }},
	};

	const m::user user
	{
		user_id
	};

	char displayname_buf[256];
	const string_view displayname
	{
		user.profile(displayname_buf, "displayname")
	};

	char avatar_url_buf[256];
	const string_view avatar_url
	{
		user.profile(avatar_url_buf, "avatar_url")
	};

	const json::iov::add_if add_if[]
	{
		{ content,  !empty(displayname),  { "displayname",  displayname  }},
		{ content,  !empty(avatar_url),   { "avatar_url",    avatar_url  }},
	};

	return commit(room, event, content);
}

event::id::buf
join__alias_user(const m::room::alias &room_alias,
                 const m::user::id &user_id)
{
	const room::id::buf room_id
	{
		m::room_id(room_alias)
	};

	if(!exists(room_id))
		return bootstrap(room_alias, room_id, user_id);

	return join__room_user(room_id, user_id);
}

static event::id::buf
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

	const m::user user
	{
		user_id
	};

	char displayname_buf[256];
	const string_view displayname
	{
		user.profile(displayname_buf, "displayname")
	};

	char avatar_url_buf[256];
	const string_view avatar_url
	{
		user.profile(avatar_url_buf, "avatar_url")
	};

	const json::iov::add_if add_if[]
	{
		{ content,  !empty(displayname),  { "displayname",  displayname  }},
		{ content,  !empty(avatar_url),   { "avatar_url",    avatar_url  }},
	};

	m::vm::opts::commit opts;
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

	return event_id;
}
