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
	static void _rooms_polylog_room(data &, const m::room &);
	static void _rooms_polylog(data &, const string_view &membership);
	static void rooms_polylog(data &);

	static void _rooms_linear(data &, const string_view &membership);
	static void rooms_linear(data &);

	extern item rooms;
}

decltype(ircd::m::sync::rooms)
ircd::m::sync::rooms
{
	"rooms",
	rooms_polylog,
	rooms_linear
};

void
ircd::m::sync::rooms_linear(data &data)
{
	json::stack::object object{data.out};
	_rooms_linear(data, "invite");
	_rooms_linear(data, "join");
	_rooms_linear(data, "leave");
	_rooms_linear(data, "ban");
}

void
ircd::m::sync::_rooms_linear(data &data,
                             const string_view &membership)
{
	const scope_restore<decltype(data.membership)> theirs
	{
		data.membership, membership
	};

	m::sync::for_each("rooms", [&](item &item)
	{
		json::stack::member member
		{
			data.out, item.member_name()
		};

		item.linear(data);
		return true;
	});
}

void
ircd::m::sync::rooms_polylog(data &data)
{
	json::stack::object object{data.out};
	_rooms_polylog(data, "invite");
	_rooms_polylog(data, "join");
	_rooms_polylog(data, "leave");
	_rooms_polylog(data, "ban");
	return;
}

void
ircd::m::sync::_rooms_polylog(data &data,
                              const string_view &membership)
{
	const scope_restore<decltype(data.membership)> theirs
	{
		data.membership, membership
	};

	json::stack::object object
	{
		data.out, membership
	};

	data.user_rooms.for_each(membership, [&data]
	(const m::room &room, const string_view &membership_)
	{
		#ifdef RB_DEBUG
		sync::stats stats
		{
			data.stats?
				*data.stats:
				sync::stats{}
		};

		if(data.stats)
			stats.timer = timer{};
		#endif

		_rooms_polylog_room(data, room);

		#ifdef RB_DEBUG
		thread_local char tmbuf[32];
		if(data.stats && bool(debug_stats)) log::debug
		{
			log, "polylog %s %s in %s",
			loghead(data),
			string_view{room.room_id},
			ircd::pretty(tmbuf, stats.timer.at<milliseconds>(), true)
		};
		#endif
	});
}

void
ircd::m::sync::_rooms_polylog_room(data &data,
                                   const m::room &room)
{
	const scope_restore<decltype(data.room)> theirs
	{
		data.room, &room
	};

	json::stack::object object
	{
		data.out, room.room_id
	};

	m::sync::for_each("rooms", [&data]
	(item &item)
	{
		json::stack::member member
		{
			data.out, item.member_name()
		};

		item.polylog(data);
		return true;
	});
}
