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
post__forget(client &client,
             const resource::request &request,
             const m::room::id &room_id)
{
	const m::room room
	{
		room_id
	};

	const m::user::room user_room
	{
		request.user_id
	};

	const auto event_idx
	{
		user_room.get(std::nothrow, "ircd.member", room_id)
	};

	if(!event_idx)
		throw m::NOT_FOUND
		{
			"No user membership found for room %s.",
			string_view{room_id}
		};

	char room_membuf[32];
	const string_view &room_membership
	{
		room.membership(room_membuf, request.user_id)
	};

	char user_membuf[32];
	string_view user_membership;
	m::get(std::nothrow, event_idx, "content", [&user_membuf, &user_membership]
	(const json::object &content)
	{
		const json::string &membership
		{
			content.get("membership")
		};

		user_membership =
		{
			user_membuf, copy(user_membuf, membership)
		};
	});

	if(room_membership != user_membership)
		log::critical
		{
			"Membership mismatch for user %s in room %s: user:%s vs. room:%s",
			string_view{request.user_id},
			string_view{room_id},
			user_membership,
			room_membership
		};

	if(user_membership != "leave")
		throw m::error
		{
			http::OK, "M_MEMBERSHIP_NOT_LEAVE",
			"You must leave the room first to forget it"
		};

	const auto event_id
	{
		m::event_id(event_idx)
	};

	const auto redact_id
	{
		redact(user_room, request.user_id, event_id, "forget")
	};

	return resource::response
	{
		client, http::OK
	};
}
