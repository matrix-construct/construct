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

using namespace ircd;

extern "C" m::event::id::buf
invite__room_user(const m::room &,
                  const m::user::id &target,
                  const m::user::id &sender);

resource::response
post__invite(client &client,
             const resource::request &request,
             const m::room::id &room_id)
{
	const m::user::id &target
	{
		unquote(request.at("user_id"))
	};

	const m::user::id &sender
	{
		request.user_id
	};

	const auto event_id
	{
		invite__room_user(room_id, target, sender)
	};

	return resource::response
	{
		client, http::OK
	};
}

m::event::id::buf
invite__room_user(const m::room &room,
                  const m::user::id &target,
                  const m::user::id &sender)
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
		{ event,    { "sender",      sender           }},
		{ event,    { "state_key",   target           }},
		{ event,    { "membership",  "invite"         }},
		{ content,  { "membership",  "invite"         }},
	};

	return commit(room, event, content);
}
