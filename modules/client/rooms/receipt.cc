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
post__receipt(client &client,
              const resource::request &request,
              const m::room::id &room_id)
{
	if(request.parv.size() < 3)
		throw m::NEED_MORE_PARAMS
		{
			"receipt type required"
		};

	if(request.parv.size() < 4)
		throw m::NEED_MORE_PARAMS
		{
			"event_id required"
		};

	const string_view &receipt_type
	{
		request.parv[2]
	};

	m::event::id::buf event_id
	{
		url::decode(event_id, request.parv[3])
	};

	const bool useful
	{
		// Check if event_id is more recent than the last receipt's event_id.
		// We currently don't do anything with receipts targeting the past.
		m::receipt::freshest(room_id, request.user_id, event_id) &&

		// Check if user wants to prevent sending receipts to this room.
		!m::receipt::ignoring(request.user_id, room_id) &&

		// Check if user wants to prevent based on this event's specifics.
		!m::receipt::ignoring(request.user_id, event_id)
	};

	if(useful)
		m::receipt::read(room_id, request.user_id, event_id);

	return resource::response
	{
		client, http::OK
	};
}
