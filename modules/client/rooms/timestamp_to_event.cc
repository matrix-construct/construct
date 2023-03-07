// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2023 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include "rooms.h"

using namespace ircd;

m::resource::response
get__timestamp_to_event(client &client,
                        const m::resource::request &request,
                        const m::room::id &room_id)
{
	const auto &dir
	{
		request.query["dir"]
	};

	const milliseconds ts
	{
		request.query.at<long>("ts")
	};

	const m::event::idx event_idx
	{
		0UL
	};

	const m::event::fetch event
	{
		std::nothrow, event_idx
	};

	const long &event_ts
	{
		json::get<"origin_server_ts"_>(event)
	};

	const m::room room
	{
		room_id, event.event_id
	};

	if(!visible(room, request.user_id))
		throw m::ACCESS_DENIED
		{
			"You are not permitted to view the room at this event"
		};

	return m::resource::response
	{
		client, http::NOT_IMPLEMENTED, json::members
		{
			{ "event_id", event.event_id },
			{ "origin_server_ts", event_ts },
		}
	};
}
