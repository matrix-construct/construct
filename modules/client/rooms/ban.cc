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
post__ban(client &client,
          const resource::request &request,
          const m::room::id &room_id)
{
	const m::user::id &user_id
	{
		unquote(request.at("user_id"))
	};

	const json::string &reason
	{
		request["reason"]
	};

	// Power levels will be checked again at some point during eval, however
	// it's fine to just check first and avoid all of the eval machinery. This
	// data is also cached.
	const m::room room{room_id};
	const m::room::power power{room};
	if(!power(request.user_id, "ban"))
		throw m::ACCESS_DENIED
		{
			"Your power level (%ld) is not high enough for ban (%ld)",
			power.level_user(request.user_id),
			power.level("ban")
		};

	const auto event_id
	{
		send(room, request.user_id, "m.room.member", user_id,
		{
			{ "membership",  "ban"   },
			{ "reason",      reason  },
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
