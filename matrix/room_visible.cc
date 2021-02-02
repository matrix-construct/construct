// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m
{
	static bool visible_to_node(const room &, const string_view &node_id, const event &);
	static bool visible_to_user(const room &, const string_view &history_visibility, const m::user::id &, const event &);
}

bool
ircd::m::visible(const m::event &event,
                 const string_view &mxid)
{
	const m::room room
	{
		at<"room_id"_>(event), event.event_id
	};

	const m::room::state state
	{
		room
	};

	const event::idx visibility_event_idx
	{
		state.get(std::nothrow, "m.room.history_visibility", "")
	};

	char buf[32];
	string_view history_visibility{"shared"};
	m::get(std::nothrow, visibility_event_idx, "content", [&buf, &history_visibility]
	(const json::object &content)
	{
		const json::string &_history_visibility
		{
			content.get("history_visibility", "shared")
		};

		history_visibility = strncpy
		{
			buf, _history_visibility
		};
	});

	if(history_visibility == "world_readable")
		return true;

	if(empty(mxid))
		return false;

	if(rfc3986::valid_remote(std::nothrow, mxid))
		return visible_to_node(room, mxid, event);

	if(m::valid(m::id::USER, mxid))
	{
		const m::user::id user_id
		{
			mxid
		};

		if(visible_to_user(room, history_visibility, user_id, event))
			return true;

		// Unrestricted visibility for opers
		if(is_oper(user_id))
			return true;

		return false;
	}

	throw m::UNSUPPORTED
	{
		"Cannot determine visibility of %s for '%s'",
		string_view{room.room_id},
		mxid,
	};
}

bool
ircd::m::visible_to_user(const m::room &room,
                         const string_view &history_visibility,
                         const m::user::id &user_id,
                         const m::event &event)
{
	assert(history_visibility != "world_readable");

	// Allow any member event where the state_key string is a user mxid.
	if(json::get<"type"_>(event) == "m.room.member")
		if(at<"state_key"_>(event) == user_id)
			return true;

	// Get the membership of the user in the room at the event.
	char buf[m::room::MEMBERSHIP_MAX_SIZE];
	const string_view membership
	{
		m::membership(buf, room, user_id)
	};

	if(membership == "join")
		return true;

	if(history_visibility == "joined")
		return false;

	if(membership == "invite")
		return true;

	if(history_visibility == "invited")
		return false;

	// The history_visibility is now likely "shared"; though we cannot assert
	// that in case some other string is used for any non-spec customization
	// or for graceful forward compatibility. We default to "shared" here.
	//assert(history_visibility == "shared");

	// If the room is not at the present event then we have to run another
	// test for membership here. Otherwise the "join" test already failed.
	if(!room.event_id)
		return false;

	// An m::room instance with no event_id is used to query the room at the
	// present state.
	const m::room present
	{
		room.room_id
	};

	return m::membership(present, user_id, m::membership_positive); // join || invite
}

bool
ircd::m::visible_to_node(const m::room &room,
                         const string_view &node_id,
                         const m::event &event)
{
	// Allow auth chain events XXX: this is too broad
	if(m::room::auth::is_power_event(event))
		return true;

	// Allow any event where the state_key string is a user mxid and the server
	// is the host of that user. Note that applies to any type of event.
	if(m::valid(m::id::USER, json::get<"state_key"_>(event)))
		if(m::user::id(at<"state_key"_>(event)).host() == node_id)
			return true;

	const m::room::origins origins
	{
		room
	};

	// Allow joined servers
	if(origins.has(node_id))
		return true;

	return false;
}
