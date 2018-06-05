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

extern "C" event::id::buf
redact__(const room &room,
         const id::user &sender,
         const id::event &event_id,
         const string_view &reason);

resource::response
put__redact(client &client,
            const resource::request &request,
            const room::id &room_id)
{
	if(request.parv.size() < 3)
		throw NEED_MORE_PARAMS
		{
			"event_id parameter missing"
		};

	event::id::buf redacts
	{
		url::decode(request.parv[2], redacts)
	};

	if(request.parv.size() < 4)
		throw NEED_MORE_PARAMS
		{
			"txnid parameter missing"
		};

	const string_view &txnid
	{
		request.parv[3]
	};

	const room room
	{
		room_id
	};

	const auto &reason
	{
		unquote(request["reason"])
	};

	const auto event_id
	{
		redact__(room, request.user_id, redacts, reason)
	};

	return resource::response
	{
		client, json::members
		{
			{ "event_id", event_id }
		}
	};
}

resource::response
post__redact(client &client,
             const resource::request &request,
             const room::id &room_id)
{
	if(request.parv.size() < 3)
		throw NEED_MORE_PARAMS
		{
			"event_id parameter missing"
		};

	event::id::buf redacts
	{
		url::decode(request.parv[2], redacts)
	};

	const room room
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

event::id::buf
redact__(const room &room,
         const id::user &sender,
         const id::event &event_id,
         const string_view &reason)
{
	json::iov event;
	const json::iov::push push[]
	{
		{ event,    { "type",       "m.room.redaction"  }},
		{ event,    { "sender",      sender             }},
		{ event,    { "redacts",     event_id           }},
	};

	json::iov content;
	const json::iov::set _reason
	{
		content, !empty(reason),
		{
			"reason", [&reason]() -> json::value
			{
				return reason;
			}
		}
	};

	return commit(room, event, content);
}
