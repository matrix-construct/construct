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

resource::response
post__unban(client &client,
            const resource::request &request,
            const m::room::id &room_id)
{
	const m::user::id &user_id
	{
		unquote(request.at("user_id"))
	};

	const json::string &reason // non-spec, but wth
	{
		request["reason"]
	};

	const m::room room
	{
		room_id
	};

	// These items will be checked again during eval for an atomic
	// determination of whether this request will go through. However
	// we can save a lot of trouble by testing these conditions first
	// out here and erroring early; this also warms the cache for eval.

	const m::room::power power{room};
	if(!power(request.user_id, "ban"))
		throw m::ACCESS_DENIED
		{
			"Your power level (%ld) is not high enough for ban (%ld) so you cannot unban.",
			power.level_user(request.user_id),
			power.level("ban")
		};

	if(!room.membership(user_id, "ban"))
		throw m::error
		{
			http::OK, "M_BAD_STATE",
			"User %s is not banned from room %s",
			string_view{user_id},
			string_view{room_id}
		};

	const auto event_id
	{
		send(room, request.user_id, "m.room.member", user_id,
		{
			{ "membership", "leave" },
			{ "reason",     reason  },
		})
	};

	return resource::response
	{
		client, http::OK, json::members
		{
			{ "event_id", event_id }
		}
	};
}
