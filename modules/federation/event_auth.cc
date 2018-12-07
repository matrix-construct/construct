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
	"federation event_auth (undocumented)"
};

resource
event_auth_resource
{
	"/_matrix/federation/v1/event_auth/",
	{
		"federation event_auth",
		resource::DIRECTORY,
	}
};

conf::item<size_t>
event_auth_flush_hiwat
{
	{ "name",     "ircd.federation.event_auth.flush.hiwat" },
	{ "default",  16384L                                   },
};

resource::response
get__event_auth(client &client,
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

	if(request.parv.size() < 2)
		throw m::NEED_MORE_PARAMS
		{
			"event_id path parameter required"
		};

	m::event::id::buf event_id
	{
		url::decode(event_id, request.parv[1])
	};

	const m::event::fetch event
	{
		event_id
	};

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
		response.buf, response.flusher(), size_t(event_auth_flush_hiwat)
	};

	json::stack::object top{out};
	json::stack::member auth_chain_m
	{
		top, "auth_chain"
	};

	json::stack::array auth_chain
	{
		auth_chain_m
	};

	const auto append{[&auth_chain]
	(const m::event &event)
	{
		if(json::get<"event_id"_>(event))
			auth_chain.append(event);
	}};

	const auto append_with_sender{[&append, &state, &event]
	(const m::event &event_)
	{
		append(event_);
		if(json::get<"sender"_>(event_) != json::get<"sender"_>(event))
			state.get(std::nothrow, "m.room.member", json::get<"sender"_>(event_), append);
	}};

	state.get(std::nothrow, "m.room.create", "", append);
	state.get(std::nothrow, "m.room.join_rules", "", append);
	state.get(std::nothrow, "m.room.power_levels", "", append_with_sender);
	state.get(std::nothrow, "m.room.member", json::get<"sender"_>(event), append);
	return {};
}

resource::method
method_get
{
	event_auth_resource, "GET", get__event_auth,
	{
		method_get.VERIFY_ORIGIN
	}
};
