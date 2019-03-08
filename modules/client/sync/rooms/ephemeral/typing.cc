// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

ircd::mapi::header
IRCD_MODULE
{
    "Client Sync :Room Ephemeral :Typing"
};

namespace ircd::m::sync
{
	static bool room_ephemeral_m_typing_polylog(data &);
	static bool room_ephemeral_m_typing_linear(data &);
	extern item room_ephemeral_m_typing;
}

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
	if(data.event_idx)
		return false;

	assert(data.event);
	if(json::get<"type"_>(*data.event) != "m.typing")
		return false;

	const m::room room
	{
		json::get<"room_id"_>(*data.event)
	};

	if(!room.membership(data.user, "join"))
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
		*data.out, room.room_id
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

	json::stack::member
	{
		object, "content", json::get<"content"_>(*data.event)
	};

	return true;
}

bool
ircd::m::sync::room_ephemeral_m_typing_polylog(data &data)
{
	return false;
}
