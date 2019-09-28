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
post__forget(client &client,
             const m::resource::request &request,
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

	if(m::membership(event_idx, m::membership_positive))
		throw m::error
		{
			http::UNPROCESSABLE_ENTITY, "M_MEMBERSHIP_POSITIVE",
			"You must leave or be banned from the room to forget it."
		};

	const auto event_id
	{
		m::event_id(event_idx)
	};

	const auto redact_id
	{
		redact(user_room, request.user_id, event_id, "forget")
	};

	return m::resource::response
	{
		client, http::OK
	};
}
