// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m::sync
{
	static bool room_ephemeral_m_typing_polylog(data &);
	static bool room_ephemeral_m_typing_linear(data &);
	extern item room_ephemeral_m_typing;
}

ircd::mapi::header
IRCD_MODULE
{
    "Client Sync :Room Ephemeral :Typing"
};

decltype(ircd::m::sync::room_ephemeral_m_typing)
ircd::m::sync::room_ephemeral_m_typing
{
	"rooms.ephemeral.m_typing",
	room_ephemeral_m_typing_polylog,
	room_ephemeral_m_typing_linear
};

bool
ircd::m::sync::room_ephemeral_m_typing_linear(data &data)
{
	if(json::get<"type"_>(*data.event) != "ircd.typing")
		return false;

	const m::room target_room
	{
		unquote(json::get<"content"_>(*data.event).get("room_id"))
	};

	// Check if our user is a member of the room targetted by the typing notif
	if(!membership(target_room, data.user, "join"))
		return false;

	const m::user::id &sender
	{
		json::get<"sender"_>(*data.event)
	};

	const m::user::room user_room
	{
		sender
	};

	// Check if the ircd.typing event was sent to the user's user room,
	// and not just to any room.
	if(json::get<"room_id"_>(*data.event) != user_room.room_id)
		return false;

	// Check if the user does not want to receive typing events for this room.
	if(!m::typing::allow(data.user, *data.room, "sync"))
		return false;

	json::stack::object rooms
	{
		*data.out, "rooms"
	};

	json::stack::object membership_
	{
		*data.out, "join"
	};

	json::stack::object room_
	{
		*data.out, target_room.room_id
	};

	json::stack::object ephemeral
	{
		*data.out, "ephemeral"
	};

	json::stack::array events
	{
		*data.out, "events"
	};

	json::stack::object object
	{
		*data.out
	};

	json::stack::member
	{
		object, "type", "m.typing"
	};

	json::stack::object content
	{
		object, "content"
	};

	json::stack::array user_ids
	{
		content, "user_ids"
	};

	m::typing::for_each([&user_ids, &target_room]
	(const auto &typing)
	{
		if(json::get<"room_id"_>(typing) != target_room)
			return true;

		user_ids.append(json::get<"user_id"_>(typing));
		return true;
	});

	return true;
}

bool
ircd::m::sync::room_ephemeral_m_typing_polylog(data &data)
{
	return false;
}
