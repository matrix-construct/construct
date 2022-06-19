// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2022 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include "rooms.h"

namespace ircd::m
{
	extern conf::item<size_t> hierarchy_limit_default;
	extern conf::item<size_t> hierarchy_depth_default;
}

decltype(ircd::m::hierarchy_limit_default)
ircd::m::hierarchy_limit_default
{
    { "name",    "ircd.client.rooms.hierarchy.limit.default" },
    { "default",  512                                        },
};

decltype(ircd::m::hierarchy_depth_default)
ircd::m::hierarchy_depth_default
{
    { "name",    "ircd.client.rooms.hierarchy.depth.default" },
    { "default",  32                                         },
};

ircd::m::resource::response
get__hierarchy(ircd::client &client,
               const ircd::m::resource::request &request,
               const ircd::m::room::id &room_id)
{
	using namespace ircd::m;
	using namespace ircd;

	const auto limit
	{
		request.query.get<ushort>("limit", size_t(hierarchy_limit_default))
	};

	const auto max_depth
	{
		request.query.get<uint8_t>("max_depth", size_t(hierarchy_depth_default))
	};

	const auto suggested_only
	{
		request.query.get<bool>("suggested_only", false)
	};

	const auto from
	{
		request.query.get<event::idx>("from", 0UL)
	};

	if(!m::exists(room_id))
		throw m::NOT_FOUND
		{
			"Cannot find hierarchy for %s which is not found.",
			string_view{room_id}
		};

	if(!m::visible(room_id, request.user_id))
		throw m::FORBIDDEN
		{
			"You are not allowed to view hierarchy of %s",
			string_view{room_id},
		};

	const m::room::state state
	{
		room_id
	};

	m::resource::response::chunked response
	{
		client, http::OK
	};

	json::stack out
	{
		response.buf, response.flusher()
	};

	json::stack::object top
	{
		out
	};

	json::stack::array array
	{
		top, "rooms"
	};

	size_t count(0);
	const auto closure{[&array, &from, &limit, &count]
	(const string_view &, const string_view &state_key, const event::idx &event_idx)
	{
		if(event_idx < from)
			return true;

		if(!m::valid(m::id::ROOM, state_key))
			return true;

		const m::room::id child_id
		{
			state_key
		};

		//TODO: XXX
		if(!m::exists(child_id))
			return true;

		json::stack::object chunk
		{
			array
		};

		m::rooms::summary::get(chunk, child_id);
		return ++count < limit;
	}};

	// riot crashes unless we start with the parent
	closure("m.space.child", room_id, 0);

	// then the children
	state.for_each("m.space.child", closure);

	// done
	return std::move(response);
}
