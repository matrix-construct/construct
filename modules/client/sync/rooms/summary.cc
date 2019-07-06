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
	"Client Sync :Room Summary"
};

namespace ircd::m::sync
{
	static bool room_summary_polylog(data &);
	static bool room_summary_linear(data &);
	extern item room_summary;
}

decltype(ircd::m::sync::room_summary)
ircd::m::sync::room_summary
{
	"rooms.summary",
	room_summary_polylog,
	room_summary_linear,
	{
		{ "initial",  true },
	}
};

bool
ircd::m::sync::room_summary_linear(data &data)
{
	if(!data.event_idx)
		return false;

	if(!data.membership)
		return false;

	if(!data.room)
		return false;

	assert(data.event);
	if(at<"type"_>(*data.event) != "m.room.member")
		return false;

	const auto &room{*data.room};
	const m::room::members members
	{
		room
	};

	json::stack::object rooms
	{
		*data.out, "rooms"
	};

	json::stack::object membership_
	{
		*data.out, data.membership
	};

	json::stack::object room_
	{
		*data.out, data.room->room_id
	};

	json::stack::object summary
	{
		*data.out, "summary"
	};

	json::stack::member
	{
		*data.out, "m.joined_member_count", json::value
		{
			long(members.count("join"))
		}
	};

	/*
	json::stack::member
	{
		*data.out, "m.invited_member_count", json::value
		{
			long(members.count("invite"))
		}
	};
	*/

	return true;
}

bool
ircd::m::sync::room_summary_polylog(data &data)
{
	const auto &room{*data.room};
	const m::room::members members{room};

	json::stack::member
	{
		*data.out, "m.joined_member_count", json::value
		{
			long(members.count("join"))
		}
	};

	/*
	json::stack::member
	{
		*data.out, "m.invited_member_count", json::value
		{
			long(members.count("invite"))
		}
	};
	*/

	return true;
}
