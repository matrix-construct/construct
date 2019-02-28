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
	assert(data.event);
	if(json::get<"type"_>(*data.event) != "m.typing")
		return false;

	json::stack::object object
	{
		*data.out
	};

	object.append(*data.event);
	return true;
}

bool
ircd::m::sync::room_ephemeral_m_typing_polylog(data &data)
{
	return false;
}
