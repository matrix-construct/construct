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

void // This is in receipt.cc; not listed in rooms.h so we declare here.
handle_receipt_m_read(client &client,
                      const resource::request &request,
                      const m::room::id &room_id,
                      const m::event::id &event_id);

static void
handle_m_fully_read(client &client,
                    const resource::request &request,
                    const m::room::id &room_id,
                    const json::string &input);

resource::response
post__read_markers(client &client,
                   const resource::request &request,
                   const m::room::id &room_id)
{
	const json::string &m_read
	{
		request["m.read"]
	};

	const json::string &m_fully_read
	{
		request["m.fully_read"]
	};

	if(m_fully_read)
		handle_m_fully_read(client, request, room_id, m_fully_read);

	if(m_read)
		handle_receipt_m_read(client, request, room_id, m_read);

	return resource::response
	{
		client, http::OK
	};
}

void
handle_m_fully_read(client &client,
                    const resource::request &request,
                    const m::room::id &room_id,
                    const json::string &input)
{
	m::event::id::buf event_id_buf;
	if(!m::valid(m::id::EVENT, input))
		event_id_buf = m::head(room_id);

	const m::event::id &event_id
	{
		event_id_buf?: m::event::id{input}
	};

	const m::user::room_account_data account_data
	{
		request.user_id, room_id
	};

	const json::strung content{json::members
	{
		{ "event_id", event_id }
	}};

	account_data.set("m.fully_read", json::object(content));
}
