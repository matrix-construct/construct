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

static resource::response
get__state(client &client,
           const resource::request &request,
           const m::room::id &room_id,
           const string_view &event_id)
{
	const m::room::state state
	{
		m::room{room_id, event_id}
	};

	std::vector<json::value> ret;
	ret.reserve(32);

	state.for_each([&ret]
	(const m::event &event)
	{
		ret.emplace_back(event);
	});

	return resource::response
	{
		client, json::value
		{
			ret.data(), ret.size()
		}
	};
}

static resource::response
get__state(client &client,
           const resource::request &request,
           const m::room::id &room_id,
           const string_view &event_id,
           const string_view &type)
{
	const m::room::state state
	{
		m::room{room_id, event_id}
	};

	std::vector<json::value> ret;
	ret.reserve(32);
	state.for_each(type, [&ret]
	(const m::event &event)
	{
		//TODO: Fix conversion derpage
		ret.emplace_back(event);
	});

	return resource::response
	{
		client, json::value
		{
			ret.data(), ret.size()
		}
	};
}

static resource::response
get__state(client &client,
           const resource::request &request,
           const m::room::id &room_id,
           const string_view &event_id,
           const string_view &type,
           const string_view &state_key)
{
	const m::room::state state
	{
		m::room{room_id, event_id}
	};

	std::array<json::value, 1> ret;
	const bool i{state.get(std::nothrow, type, state_key, [&ret]
	(const m::event &event)
	{
		ret[0] = event;
	})};

	return resource::response
	{
		client, json::value
		{
			ret.data(), i
		}
	};
}

resource::response
get__state(client &client,
           const resource::request &request,
           const m::room::id &room_id)
{
	char type_buf[uint(256 * 1.34 + 1)];
	const string_view &type
	{
		url::decode(request.parv[2], type_buf)
	};

	char skey_buf[uint(256 * 1.34 + 1)];
	const string_view &state_key
	{
		url::decode(request.parv[3], skey_buf)
	};

	// (non-standard) Allow an event_id to be passed in the query string
	// for reference framing.
	char evid_buf[uint(256 * 1.34 + 1)];
	const string_view &event_id
	{
		url::decode(request.query["event_id"], evid_buf)
	};

	if(type && state_key)
		return get__state(client, request, room_id, event_id, type, state_key);

	if(type)
		return get__state(client, request, room_id, event_id, type);

	return get__state(client, request, room_id, event_id);
}

resource::response
put__state(client &client,
           const resource::request &request,
           const m::room::id &room_id)
{
	char type_buf[uint(256 * 1.34 + 1)];
	const string_view &type
	{
		url::decode(request.parv[2], type_buf)
	};

	char skey_buf[uint(256 * 1.34 + 1)];
	const string_view &state_key
	{
		url::decode(request.parv[3], skey_buf)
	};

	const json::object &content
	{
		request.content
	};

	const auto event_id
	{
		m::send(room_id, request.user_id, type, state_key, content)
	};

	return resource::response
	{
		client, json::members
		{
			{ "event_id", event_id }
		}
	};
}
