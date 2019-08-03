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
	if(unlikely(!my(user_id)))
		throw panic
		{
			"Can only join my users."
		};

	// Branch for when nothing is known about the room.
	if(!exists(room))
	{
		// The bootstrap condcts a blocking make_join and issues a join
		// event, returning the event_id; afterward asynchronously it will
		// attempt a send_join, and then process the room events.
		m::event::id::buf ret;
		m::room::bootstrap
		{
			ret, room.room_id, user_id, room.room_id.host() //TODO: host
		};

		return ret;
	}

	if(room.membership(user_id, "join"))
	{
		const auto &event_idx
		{
			room.get(std::nothrow, "m.room.member", user_id)
		};

		const event::id::buf event_id
		{
			event_idx?
				m::event_id(event_idx, std::nothrow):
				event::id::buf{}
		};

		if(event_id)
			return event_id;
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
	if(unlikely(!my(user_id)))
		throw panic
		{
			"Can only join my users."
		};

	const room::id::buf room_id
	{
		m::room_id(room_alias)
	};

	if(!exists(room_id))
	{
		m::event::id::buf ret;
		m::room::bootstrap
		{
			ret, room_id, user_id, room_alias.host()
		};

		return ret;
	}

	const m::room room
	{
		room_id
	};

	return m::join(room, user_id);
}
