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
put__redact(client &client,
            const resource::request &request,
            const m::room::id &room_id)
{
	if(request.parv.size() < 3)
		throw m::NEED_MORE_PARAMS
		{
			"event_id parameter missing"
		};

	m::event::id::buf redacts
	{
		url::decode(request.parv[2], redacts)
	};

	if(request.parv.size() < 4)
		throw m::NEED_MORE_PARAMS
		{
			"txnid parameter missing"
		};

	const string_view &txnid
	{
		request.parv[3]
	};

	const m::room room
	{
		room_id
	};

	const auto &reason
	{
		unquote(request["reason"])
	};

	const auto event_id
	{
		redact(room, request.user_id, redacts, reason)
	};

	return resource::response
	{
		client, json::members
		{
			{ "event_id", event_id }
		}
	};
}
