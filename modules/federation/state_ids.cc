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
	"federation state_ids"
};

resource
state_ids_resource
{
	"/_matrix/federation/v1/state_ids/",
	{
		"federation state_ids",
		resource::DIRECTORY,
	}
};

resource::response
get__state_ids(client &client,
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
		response.buf, response.flusher()
	};

	json::stack::object top{out};

	// auth_chain
	if(request.query.get<bool>("auth_chain", true))
	{
		json::stack::array auth_chain
		{
			top, "auth_chain"
		};

		ac.for_each([&auth_chain]
		(const m::event::idx &event_idx)
		{
			m::event_id(event_idx, std::nothrow, [&auth_chain]
			(const auto &event_id)
			{
				auth_chain.append(event_id);
			});
		});
	}

	// pdu_ids
	if(request.query.get<bool>("pdu_ids", true))
	{
		json::stack::array pdu_ids
		{
			top, "pdu_ids"
		};

		state.for_each(m::event::id::closure{[&pdu_ids]
		(const m::event::id &event_id)
		{
			pdu_ids.append(event_id);
		}});
	}

	return response;
}

resource::method
method_get
{
	state_ids_resource, "GET", get__state_ids,
	{
		method_get.VERIFY_ORIGIN
	}
};
