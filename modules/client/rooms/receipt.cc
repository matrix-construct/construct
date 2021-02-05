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
                      const m::resource::request &request,
                      const m::room::id &room_id,
                      const m::event::id &event_id);

static conf::item<bool>
receipt_read_attention
{
	{ "name",     "ircd.m.receipt.read.attention" },
	{ "default",  false                           },
};

m::resource::response
post__receipt(client &client,
              const m::resource::request &request,
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

	return m::resource::response
	{
		client, http::OK
	};
}

void
handle_receipt_m_read(client &client,
                      const m::resource::request &request,
                      const m::room::id &room_id,
                      const m::event::id &event_id)
{
	const bool client_hidden
	{
		// Spec hidden; defaults to false.
		request.get("m.hidden", false)
	};

	const bool freshest
	{
		// If the receipt already to be marked hidden we can elide queries.
		client_hidden

		// Check if deepest receipt in the room.
		|| m::receipt::freshest(room_id, request.user_id, event_id)
	};

	const bool ignoring
	{
		// If the receipt already to be marked hidden we can elide queries.
		(client_hidden || !freshest)

		// Check if user wants to prevent sending receipts to this room.
		|| m::receipt::ignoring(request.user_id, room_id)

		// Check if user wants to prevent based on this event's specifics.
		|| m::receipt::ignoring(request.user_id, event_id)
	};

	const bool hidden
	{
		false

		// Client wants it hidden.
		|| client_hidden

		// The freshest receipt is the one on the effectively deepest event
		// in the room and what we'll end up revealing to the federation.
		|| !freshest

		// Feature to not send receipts based on configurable state in user's room
		|| ignoring
	};

	// When the receipt is not federated (marked as hidden) we don't need
	// to save it locally unless configured for advanced eye-tracking
	// features; bail out here.
	if(!receipt_read_attention && hidden)
		return;

	const bool add_hidden
	{
		// Has to be hidden to add m.hidden
		hidden

		// Don't need to add hidden if client already made it hidden
		&& !client_hidden
	};

	// The options object starts with anything in the request content, which
	// differs depending on whether this is being called from a /receipt or
	// /read_markers resource handler. The receipt::read() implementation
	// looks for properties knowing this call pattern, thus it's best to just
	// convey the whole content here for forward compat.
	json::object options
	{
		request
	};

	// Ignoring still involves creating a receipt in all respects except
	// transmitting it to local and remote parties. This behavior is achieved
	// by the m.hidden tag. If the options do not contain this tag we add it.
	std::string options_buf;
	if(add_hidden)
		options = options_buf = json::replace(options,
		{
			{ "m.hidden", true }
		});

	m::receipt::read(room_id, request.user_id, event_id, options);
}
