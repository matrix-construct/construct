// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m::sync
{
	static bool _groups_polylog(data &, const string_view &membership);
	static bool groups_polylog(data &);

	static bool _groups_linear(data &, const string_view &membership);
	static bool groups_linear(data &);

	extern item groups;
}

ircd::mapi::header
IRCD_MODULE
{
	"Client Sync :Groups"
};

decltype(ircd::m::sync::groups)
ircd::m::sync::groups
{
	"groups",
	groups_polylog,
	groups_linear,
};

bool
ircd::m::sync::groups_linear(data &data)
{
	assert(data.event);
	const m::room room
	{
		json::get<"room_id"_>(*data.event)?
			m::room::id{json::get<"room_id"_>(*data.event)}:
			m::room::id{}
	};

	const scope_restore their_room
	{
		data.room, &room
	};

	char membuf[room::MEMBERSHIP_MAX_SIZE];
	const string_view &membership
	{
		data.room?
			m::membership(membuf, room, data.user):
			string_view{}
	};

	const scope_restore their_membership
	{
		data.membership, membership
	};

	bool ret{false};

	return ret;
}

bool
ircd::m::sync::groups_polylog(data &data)
{
	bool ret{false};

	ret |= _groups_polylog(data, "join");
	if(ret)
		return ret;

	ret |= _groups_polylog(data, "invite");
	if(ret)
		return ret;

	ret |= _groups_polylog(data, "leave");
	if(ret)
		return ret;

	return ret;
}

bool
ircd::m::sync::_groups_polylog(data &data,
                              const string_view &membership)
{
	const scope_restore theirs
	{
		data.membership, membership
	};

	json::stack::object object
	{
		*data.out, membership
	};

	bool ret{false};

	return ret;
}
