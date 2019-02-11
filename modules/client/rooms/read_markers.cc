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
post__read_markers(client &client,
                   const resource::request &request,
                   const m::room::id &room_id)
{
	const string_view m_fully_read
	{
		unquote(request["m.fully_read"])
	};

	const string_view m_read
	{
		unquote(request["m.read"])
	};

	const auto &marker
	{
		m_read?: m_fully_read
	};

	m::event::id::buf head;
	if(marker) switch(m::sigil(marker))
	{
		case m::id::EVENT:
			head = marker;
			break;

		case m::id::ROOM:
			head = m::head(m::room::id(marker));
			break;

		default: log::dwarning
		{
			"Unhandled read marker '%s' sigil type",
			string_view{marker}
		};
	}

	const bool useful
	{
		head &&

		// Check if the marker is more recent than the last marker they sent.
		// We currently don't do anything with markers targeting the past
		m::receipt::freshest(room_id, request.user_id, head) &&

		// Check if the user wants to prevent sending a receipt to the room.
		!m::receipt::ignoring(request.user_id, room_id) &&

		// Check if the user wants to prevent based on this event's specifics.
		!m::receipt::ignoring(request.user_id, head)
	};

	if(useful)
		m::receipt::read(room_id, request.user_id, head);

	return resource::response
	{
		client, http::OK
	};
}
