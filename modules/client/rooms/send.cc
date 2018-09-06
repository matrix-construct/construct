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

using namespace ircd::m;
using namespace ircd;

resource::response
put__send(client &client,
          const resource::request &request,
          const room::id &room_id)
{
	if(request.parv.size() < 3)
		throw NEED_MORE_PARAMS
		{
			"type parameter missing"
		};

	const string_view &type
	{
		request.parv[2]
	};

	if(request.parv.size() < 4)
		throw NEED_MORE_PARAMS
		{
			"txnid parameter missing"
		};

	const string_view &transaction_id
	{
		request.parv[3]
	};

	m::vm::copts copts;
	copts.client_txnid = transaction_id;

	room room
	{
		room_id, &copts
	};

	const json::object &content
	{
		request.content
	};

	const auto event_id
	{
		send(room, request.user_id, type, content)
	};

	return resource::response
	{
		client, json::members
		{
			{ "event_id", event_id }
		}
	};
}

extern "C" event::id::buf
send__iov(const room &room,
          const id::user &sender,
          const string_view &type,
          const json::iov &content)
{
	json::iov event;
	const json::iov::push push[]
	{
		{ event,    { "sender",  sender  }},
		{ event,    { "type",    type    }},
	};

	return commit(room, event, content);
}
