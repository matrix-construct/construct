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
kick(const m::room &room,
     const m::id::user &sender,
     const m::id::user &target,
     const string_view &reason);

resource::response
post__kick(client &client,
           const resource::request &request,
           const m::room::id &room_id)
{
	const m::user::id &user_id
	{
		unquote(request.at("user_id"))
	};

	const string_view &reason
	{
		unquote(request["reason"])
	};

	const m::room room
	{
		room_id
	};

	const m::room::power power
	{
		room
	};

	// Power levels will be checked again at some point during eval, however
	// it's fine to just check first and avoid all of the eval machinery. This
	// data is also cached.
	if(!power(request.user_id, "kick"))
		throw m::ACCESS_DENIED
		{
			"Your power level (%ld) is not high enough for kick (%ld)",
			power.level_user(request.user_id),
			power.level("kick")
		};

	const auto eid
	{
		kick(room, request.user_id, user_id, reason)
	};

	return resource::response
	{
		client, http::OK
	};
}

m::event::id::buf
kick(const m::room &room,
     const m::id::user &sender,
     const m::id::user &target,
     const string_view &reason)
{
	json::iov event, content;
	const json::iov::push push[]
	{
		{ event,    { "type",        "m.room.member"  }},
		{ event,    { "sender",       sender          }},
		{ content,  { "membership",   "leave"         }},
	};

	const json::iov::add reason_
	{
		content, !empty(reason),
		{
			"reason", [&reason]() -> json::value
			{
				return reason;
			}
		}
	};

	return commit(room, event, content);
}
