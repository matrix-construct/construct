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

void
handle_receipt_m_read(client &client,
                      const resource::request &request,
                      const m::room::id &room_id,
                      const m::event::id &event_id);

resource::response
post__receipt(client &client,
              const resource::request &request,
              const m::room::id &room_id)
{
	if(request.parv.size() < 3)
		throw m::NEED_MORE_PARAMS
		{
			"receipt type required"
		};

	const string_view &receipt_type
	{
		request.parv[2]
	};

	if(receipt_type == "m.read")
	{
		if(request.parv.size() < 4)
			throw m::NEED_MORE_PARAMS
			{
				"event_id required"
			};

		m::event::id::buf event_id
		{
			url::decode(event_id, request.parv[3])
		};

		handle_receipt_m_read(client, request, room_id, event_id);
	}
	else throw m::UNSUPPORTED
	{
		"Sorry, receipt type '%s' is not supported here.",
		receipt_type
	};

	return resource::response
	{
		client, http::OK
	};
}

void
handle_receipt_m_read(client &client,
                      const resource::request &request,
                      const m::room::id &room_id,
                      const m::event::id &event_id)
{
	// Check if event_id is more recent than the last receipt's event_id.
	// We currently don't do anything with receipts targeting the past.
	if(!m::receipt::freshest(room_id, request.user_id, event_id))
		return;

	// Check if user wants to prevent sending receipts to this room.
	if(m::receipt::ignoring(request.user_id, room_id))
		return;

	// Check if user wants to prevent based on this event's specifics.
	if(m::receipt::ignoring(request.user_id, event_id))
		return;

	m::receipt::read(room_id, request.user_id, event_id);
}
