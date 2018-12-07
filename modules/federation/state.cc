// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

using namespace ircd;

mapi::header
IRCD_MODULE
{
	"federation state"
};

resource
state_resource
{
	"/_matrix/federation/v1/state/",
	{
		"federation state",
		resource::DIRECTORY,
	}
};

conf::item<size_t>
state_flush_hiwat
{
	{ "name",     "ircd.federation.state.flush.hiwat" },
	{ "default",  16384L                              },
};

resource::response
get__state(client &client,
           const resource::request &request)
{
	m::room::id::buf room_id
	{
		url::decode(room_id, request.parv[0])
	};

	m::event::id::buf event_id;
	if(request.query["event_id"])
		event_id = url::decode(event_id, request.query.at("event_id"));

	const m::room room
	{
		room_id, event_id
	};

	if(!room.visible(request.node_id))
		throw m::ACCESS_DENIED
		{
			"You are not permitted to view the room at this event"
		};

	const m::room::state state
	{
		room
	};

	resource::response::chunked response
	{
		client, http::OK
	};

	json::stack out
	{
		response.buf, response.flusher(), size_t(state_flush_hiwat)
	};

	json::stack::object top{out};
	json::stack::member pdus_m
	{
		top, "pdus"
	};

	json::stack::array pdus
	{
		pdus_m
	};

	state.for_each([&pdus]
	(const m::event &event)
	{
		pdus.append(event);
	});

	return {};
}

resource::method
method_get
{
	state_resource, "GET", get__state,
	{
		method_get.VERIFY_ORIGIN
	}
};
