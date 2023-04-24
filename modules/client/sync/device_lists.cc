// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m::sync
{
	static bool device_lists_polylog(data &);
	static bool device_lists_linear(data &);

	extern item device_lists;
}

ircd::mapi::header
IRCD_MODULE
{
	"Client Sync :Device Lists"
};

decltype(ircd::m::sync::device_lists)
ircd::m::sync::device_lists
{
	"device_lists",
	device_lists_polylog,
	device_lists_linear
};

bool
ircd::m::sync::device_lists_linear(data &data)
{
	if(!data.event_idx)
		return false;

	assert(data.event);
	const m::event &event{*data.event};

	const bool including
	{
		false
		|| startswith(json::get<"type"_>(event), "ircd.device")
		|| startswith(json::get<"type"_>(event), "ircd.keys.signatures")
	};

	const bool excluding
	{
		false
		|| startswith(json::get<"type"_>(event), "ircd.device.one_time_key")
	};

	if(!including || excluding)
		return false;

	const m::user sender
	{
		m::user::id(json::get<"sender"_>(event))
	};

	if(!m::user::room::is(json::get<"room_id"_>(event), sender))
		return false;

	const m::user::mitsein mitsein
	{
		sender
	};

	const bool changed
	{
		false
		|| sender == data.user.user_id
		|| mitsein.has(data.user, "join")
	};

	const bool left
	{
		false //TODO: XXX
	};

	if(!changed && !left)
		return false;

	json::stack::object device_lists
	{
		*data.out, "device_lists"
	};

	json::stack::array array
	{
		*data.out, left? "left": "changed"
	};

	array.append(sender.user_id);
	return true;
}

bool
ircd::m::sync::device_lists_polylog(data &data)
{
	// c2s r0.6.0 13.11.5.3 sez:
	// "Note: only present on an incremental sync."
	if(!data.range.first)
		return false;

	//TODO: XXX
	return false;
}
