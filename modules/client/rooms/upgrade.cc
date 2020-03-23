// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include "rooms.h"

using namespace ircd;

m::resource::response
post__upgrade(client &client,
              const m::resource::request &request,
              const m::room::id &room_id)
{
	const json::string &new_version
	{
		request["new_version"]
	};

	const m::room room
	{
		room_id
	};

	const m::room::power power
	{
		room
	};

	if(!power(request.user_id, "event", "m.room.tombstone", ""))
		throw m::ACCESS_DENIED
		{
			"Your power level (%ld) is not high enough for "
			"m.room.tombstone (%ld) thus you cannot upgrade the room.",
			power.level_user(request.user_id),
			power.level_event("m.room.tombstone", ""),
		};

	throw m::UNSUPPORTED
	{
		"Not yet implemented."
	};

	const m::room::id replacement_room_id
	{

	};

	return m::resource::response
	{
		client, http::OK, json::members
		{
			{ "replacement_room", replacement_room_id }
		}
	};
}
