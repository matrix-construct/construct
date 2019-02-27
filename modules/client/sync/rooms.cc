// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

ircd::mapi::header
IRCD_MODULE
{
	"Client Sync :Rooms"
};

namespace ircd::m::sync
{
	static bool _rooms_polylog_room(data &, const m::room &);
	static bool _rooms_polylog(data &, const string_view &membership);
	static bool rooms_polylog(data &);

	static bool _rooms_linear(data &, const string_view &membership);
	static bool rooms_linear(data &);

	extern item rooms;
}

decltype(ircd::m::sync::rooms)
ircd::m::sync::rooms
{
	"rooms",
	rooms_polylog,
	rooms_linear
};

bool
ircd::m::sync::rooms_linear(data &data)
{
	assert(data.event);
	const auto &event{*data.event};
	if(!json::get<"room_id"_>(event))
		return false;

	const m::room room
	{
		json::get<"room_id"_>(event)
	};

	const scope_restore their_room
	{
		data.room, &room
	};

	char membuf[32];
	const string_view &membership
	{
		data.room->membership(membuf, data.user)
	};

	if(!membership)
		return false;

	const scope_restore their_membership
	{
		data.membership, membership
	};

	const event::idx room_head
	{
		head_idx(std::nothrow, *data.room)
	};

	const scope_restore their_head
	{
		data.room_head, room_head
	};

	json::stack::checkpoint checkpoint
	{
		*data.out
	};

	json::stack::object membership_
	{
		*data.out, membership
	};

	json::stack::object room_
	{
		*data.out, data.room->room_id
	};

	bool ret(false);
	m::sync::for_each("rooms", [&data, &ret]
	(item &item)
	{
		json::stack::checkpoint checkpoint
		{
			*data.out
		};

		json::stack::object object
		{
			*data.out, item.member_name()
		};

		if(item.linear(data))
			ret = true;
		else
			checkpoint.rollback();

		return !ret;
	});

	if(!ret)
		checkpoint.rollback();

	return ret;
}

bool
ircd::m::sync::rooms_polylog(data &data)
{
	bool ret{false};
	ret |= _rooms_polylog(data, "invite");
	ret |= _rooms_polylog(data, "join");
	ret |= _rooms_polylog(data, "leave");
	ret |= _rooms_polylog(data, "ban");
	return ret;
}

bool
ircd::m::sync::_rooms_polylog(data &data,
                              const string_view &membership)
{
	const scope_restore<decltype(data.membership)> theirs
	{
		data.membership, membership
	};

	json::stack::checkpoint checkpoint
	{
		*data.out
	};

	json::stack::object object
	{
		*data.out, membership
	};

	bool ret{false};
	data.user_rooms.for_each(membership, [&data, &ret]
	(const m::room &room, const string_view &membership_)
	{
		#if defined(RB_DEBUG) && 0
		sync::stats stats
		{
			data.stats?
				*data.stats:
				sync::stats{}
		};

		if(data.stats)
			stats.timer = timer{};
		#endif

		ret |= _rooms_polylog_room(data, room);

		#if defined(RB_DEBUG) && 0
		thread_local char tmbuf[32];
		if(data.stats && bool(stats_debug)) log::debug
		{
			log, "polylog %s %s in %s",
			loghead(data),
			string_view{room.room_id},
			ircd::pretty(tmbuf, stats.timer.at<milliseconds>(), true)
		};
		#endif
	});

	if(!ret)
		checkpoint.rollback();

	return ret;
}

bool
ircd::m::sync::_rooms_polylog_room(data &data,
                                   const m::room &room)
{
	const scope_restore<decltype(data.room)> theirs
	{
		data.room, &room
	};

	json::stack::checkpoint checkpoint
	{
		*data.out
	};

	json::stack::object object
	{
		*data.out, room.room_id
	};

	const event::idx room_head
	{
		head_idx(std::nothrow, room)
	};

	const scope_restore<decltype(data.room_head)> their_head
	{
		data.room_head, room_head
	};

	bool ret{false};
	m::sync::for_each("rooms", [&data, &ret]
	(item &item)
	{
		json::stack::checkpoint checkpoint
		{
			*data.out
		};

		json::stack::object object
		{
			*data.out, item.member_name()
		};

		if(item.polylog(data))
			ret = true;
		else
			checkpoint.rollback();

		return true;
	});

	if(!ret)
		checkpoint.rollback();

	return ret;
}
