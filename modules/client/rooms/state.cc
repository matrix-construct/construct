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

static resource::response
get__state(client &client,
           const resource::request &request,
           const room::id &room_id,
           const string_view &event_id);

static resource::response
get__state(client &client,
           const resource::request &request,
           const room::id &room_id,
           const string_view &event_id,
           const string_view &type);

static resource::response
get__state(client &client,
           const resource::request &request,
           const room::id &room_id,
           const string_view &event_id,
           const string_view &type,
           const string_view &state_key);

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
	char evid_buf[m::id::MAX_SIZE];
	const string_view &event_id
	{
		url::decode(evid_buf, request.query["event_id"])
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

resource::response
get__state(client &client,
           const resource::request &request,
           const room::id &room_id,
           const string_view &event_id)
{
	const room::state state
	{
		room{room_id, event_id}
	};

	std::vector<json::value> ret;
	ret.reserve(32);

	state.for_each([&ret]
	(const event &event)
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

resource::response
get__state(client &client,
           const resource::request &request,
           const room::id &room_id,
           const string_view &event_id,
           const string_view &type)
{
	const room::state state
	{
		room{room_id, event_id}
	};

	std::vector<json::value> ret;
	ret.reserve(32);
	state.for_each(type, [&ret]
	(const event &event)
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

resource::response
get__state(client &client,
           const resource::request &request,
           const room::id &room_id,
           const string_view &event_id,
           const string_view &type,
           const string_view &state_key)
{
	const room::state state
	{
		room{room_id, event_id}
	};

	std::array<json::value, 1> ret;
	const bool i{state.get(std::nothrow, type, state_key, [&ret]
	(const event &event)
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
