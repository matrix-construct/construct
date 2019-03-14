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
put__state(client &client,
           const resource::request &request,
           const room::id &room_id)
{
	if(request.parv.size() < 3)
		throw m::NEED_MORE_PARAMS
		{
			"'type' path parameter required."
		};

	char type_buf[m::event::TYPE_MAX_SIZE];
	const string_view &type
	{
		url::decode(type_buf, request.parv[2])
	};

	if(request.parv.size() < 3)
		throw m::NEED_MORE_PARAMS
		{
			"'state_key' path parameter required."
		};

	char skey_buf[m::event::STATE_KEY_MAX_SIZE];
	const string_view &state_key
	{
		url::decode(skey_buf, request.parv[3])
	};

	const json::object &content
	{
		request.content
	};

	const auto event_id
	{
		send(room_id, request.user_id, type, state_key, content)
	};

	return resource::response
	{
		client, json::members
		{
			{ "event_id", event_id }
		}
	};
}

static resource::response
get__state(client &client,
           const resource::request &request,
           const m::room::state &state);

resource::response
get__state(client &client,
           const resource::request &request,
           const room::id &room_id)
{
	char type_buf[m::event::TYPE_MAX_SIZE];
	const string_view &type
	{
		url::decode(type_buf, request.parv[2])
	};

	char skey_buf[m::event::STATE_KEY_MAX_SIZE];
	const string_view &state_key
	{
		url::decode(skey_buf, request.parv[3])
	};

	// (non-standard) Allow an event_id to be passed in the query string
	// for reference framing.
	m::event::id::buf event_id;
	if(request.query["event_id"])
		url::decode(event_id, request.query["event_id"]);

	const m::room room
	{
		room_id, event_id
	};

	if(!exists(room))
		throw m::NOT_FOUND
		{
			"Room %s not found.",
			string_view{room_id}
		};

	if(!room.visible(request.user_id))
		throw m::ACCESS_DENIED
		{
			"You are not permitted to view %s",
			string_view{room_id}
		};

	const m::room::state state
	{
		room
	};

	if(!type)
		return get__state(client, request, state);

	m::event::fetch::opts fopts;
	fopts.query_json_force = true;
	const m::event::fetch event
	{
		state.get(type, state_key), fopts
	};

	if(!visible(event, request.user_id))
		throw m::ACCESS_DENIED
		{
			"You are not permitted to view this state in %s",
			string_view{room_id}
		};

	return resource::response
	{
		client, event.source
	};
}

resource::response
get__state(client &client,
           const resource::request &request,
           const m::room::state &state)
{
	resource::response::chunked response
	{
		client, http::OK
	};

	json::stack out
	{
		response.buf, response.flusher()
	};

	json::stack::array top
	{
		out
	};

	state.for_each([&request, &top]
	(const m::event &event)
	{
		if(!visible(event, request.user_id))
			return;

		top.append(event);
	});

	return response;
}
