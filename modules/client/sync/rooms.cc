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
	static bool rooms_polylog(data &);
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
	return true;
}

bool
ircd::m::sync::rooms_polylog(data &data)
{
	json::stack::object object{data.out};
	_rooms_polylog(data, "invite");
	_rooms_polylog(data, "join");
	_rooms_polylog(data, "leave");
	_rooms_polylog(data, "ban");
	return true;
}

void
ircd::m::sync::_rooms_polylog(data &data,
                              const string_view &membership)
{
	json::stack::object object
	{
		data.out, membership
	};

	data.user_rooms.for_each(membership, [&data]
	(const m::room &room, const string_view &membership)
	{
		if(head_idx(std::nothrow, room) <= data.since)
			return;

		// Generate individual stats for this room's sync
		#ifdef RB_DEBUG
		sync::stats stats{data.stats};
		stats.timer = timer{};
		#endif

		// This scope ensures the object destructs and flushes before
		// the log message tallying the stats for this room below.
		{
			json::stack::object object
			{
				data.out, room.room_id
			};

			_rooms_polylog_room(data, room);
		}

		#ifdef RB_DEBUG
		thread_local char tmbuf[32];
		log::debug
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
try
{
	const scope_restore<decltype(data.room)> theirs
	{
		data.room, &room
	};

	m::sync::for_each("rooms", [&](item &item)
	{
		json::stack::member member
		{
			data.out, item.member_name()
		};

		item.polylog(data);
		return true;
	});
}
catch(const json::not_found &e)
{
	log::critical
	{
		log, "polylog %s room %s error :%s"
		,loghead(data)
		,string_view{room.room_id}
		,e.what()
	};
}
