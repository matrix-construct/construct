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
	if(request.parv.size() < 1)
		throw m::NEED_MORE_PARAMS
		{
			"room_id path parameter required"
		};

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

	const m::event::auth::chain ac
	{
		event_id?
			m::index(event_id):
			m::head_idx(room)
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

	// pdus
	{
		json::stack::array pdus
		{
			top, "pdus"
		};

		state.for_each([&pdus]
		(const m::event &event)
		{
			pdus.append(event);
		});
	}

	// auth_chain
	{
		json::stack::array auth_chain
		{
			top, "auth_chain"
		};

		m::event::fetch event;
		ac.for_each([&auth_chain, &event]
		(const m::event::idx &event_idx)
		{
			if(seek(event, event_idx, std::nothrow))
				auth_chain.append(event);
		});
	}

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
