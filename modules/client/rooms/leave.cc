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

m::resource::response
post__leave(client &client,
            const m::resource::request &request,
            const m::room::id &room_id)
{
	const m::room room
	{
		room_id
	};

	if(!room.has("m.room.member", request.user_id))
	{
		const m::user::room user_room
		{
			request.user_id
		};

		// In case the user's user room falls out of sync with the real room
		// state; we directly set the user's user room state.
		if(!user_room.has("ircd.member", room_id))
			throw m::error
			{
				http::NOT_MODIFIED, "M_TARGET_NOT_IN_ROOM",
				"The user %s has no membership state in %s",
				string_view{request.user_id},
				string_view{room_id},
			};

		const auto event_id
		{
			send(user_room, request.user_id, "ircd.member", room_id, json::members
			{
				{ "membership", "leave" }
			})
		};

		return m::resource::response
		{
			client, http::OK, json::members
			{
				{ "event_id", event_id }
			}
		};
	}

	const auto event_id
	{
		m::leave(room, request.user_id)
	};

	return m::resource::response
	{
		client, http::OK, json::members
		{
			{ "event_id", event_id }
		}
	};
}
