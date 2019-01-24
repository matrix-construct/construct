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

extern "C" event::id::buf
leave__room_user(const room &room,
                 const id::user &user_id);

resource::response
post__leave(client &client,
            const resource::request &request,
            const room::id &room_id)
{
	leave__room_user(room_id, request.user_id);

	return resource::response
	{
		client, http::OK
	};
}

event::id::buf
leave__room_user(const room &room,
                 const id::user &user_id)
{
	json::iov event;
	json::iov content;
	const json::iov::push push[]
	{
		{ event,    { "type",        "m.room.member"  }},
		{ event,    { "sender",      user_id          }},
		{ event,    { "state_key",   user_id          }},
		{ content,  { "membership",  "leave"          }},
	};

	return commit(room, event, content);
}
