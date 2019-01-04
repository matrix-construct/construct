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
	static void _rooms_polylog_room(data &, json::stack::object &out, const m::room &);
	static void _rooms_polylog(data &, json::stack::object &out, const string_view &membership);
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
	json::stack::object object{*data.member};
	_rooms_polylog(data, object, "invite");
	_rooms_polylog(data, object, "join");
	_rooms_polylog(data, object, "leave");
	_rooms_polylog(data, object, "ban");
	return true;
}

void
ircd::m::sync::_rooms_polylog(data &data,
                              json::stack::object &out,
                              const string_view &membership)
{
	const scope_restore<decltype(data.membership)> theirs
	{
		data.membership, membership
	};

	json::stack::member rooms_member
	{
		out, membership
	};

	json::stack::object rooms_object
	{
		rooms_member
	};

	data.user_rooms.for_each(membership, [&data, &rooms_object]
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
			json::stack::member member{rooms_object, room.room_id};
			json::stack::object object{member};
			_rooms_polylog_room(data, object, room);
		}

		#ifdef RB_DEBUG
		thread_local char iecbuf[64], rembuf[128], tmbuf[32];
		log::debug
		{
			log, "polylog %s %s %s %s wc:%zu in %s",
			string(rembuf, ircd::remote(data.client)),
			string_view{data.user.user_id},
			string_view{room.room_id},
			pretty(iecbuf, iec(data.stats.flush_bytes - stats.flush_bytes)),
			data.stats.flush_count - stats.flush_count,
			ircd::pretty(tmbuf, stats.timer.at<milliseconds>(), true)
		};
		#endif
	});
}

void
ircd::m::sync::_rooms_polylog_room(data &data,
                                   json::stack::object &out,
                                   const m::room &room)
try
{
	const scope_restore<decltype(data.room)> theirs
	{
		data.room, &room
	};

	// state
	{
		auto it(m::sync::item::map.find("rooms.$membership.$room_id.state"));
		assert(it != m::sync::item::map.end());
		const auto &item(it->second);
		assert(item);
		json::stack::member member
		{
			out, data.membership != "invite"?
				"state":
				"invite_state"
		};

		const scope_restore<decltype(data.member)> theirs
		{
			data.member, &member
		};

		item->polylog(data);
	}

	// timeline
	{
		auto it(m::sync::item::map.find("rooms.$membership.$room_id.timeline"));
		assert(it != m::sync::item::map.end());
		const auto &item(it->second);
		assert(item);
		json::stack::member member{out, "timeline"};
		const scope_restore<decltype(data.member)> theirs
		{
			data.member, &member
		};

		item->polylog(data);
	}

	// ephemeral
	{
		auto pit
		{
			m::sync::item::map.equal_range("rooms...ephemeral")
		};

		assert(pit.first != pit.second);
		json::stack::member member{out, "ephemeral"};
		json::stack::object object{member};
		const scope_restore<decltype(data.object)> theirs
		{
			data.object, &object
		};

		{
			json::stack::member member{object, "events"};
			json::stack::array array{member};
			const scope_restore<decltype(data.array)> theirs
			{
				data.array, &array
			};

			for(; pit.first != pit.second; ++pit.first)
			{
				const auto &item(pit.first->second);
				assert(item);
				item->polylog(data);
			}
		}
	}

	// account_data
	{
		auto it(m::sync::item::map.find("rooms...account_data"));
		assert(it != m::sync::item::map.end());
		const auto &item(it->second);
		assert(item);
		json::stack::member member{out, "account_data"};
		const scope_restore<decltype(data.member)> theirs
		{
			data.member, &member
		};

		item->polylog(data);
	}

	// unread_notifications
	{
		auto it(m::sync::item::map.find("rooms...unread_notifications"));
		assert(it != m::sync::item::map.end());
		const auto &item(it->second);
		assert(item);
		json::stack::member member{out, "unread_notifications"};
		const scope_restore<decltype(data.member)> theirs
		{
			data.member, &member
		};

		item->polylog(data);
	}
}
catch(const json::not_found &e)
{
	log::critical
	{
		log, "polylog sync room %s error %lu to %lu (vm @ %zu) :%s"
		,string_view{room.room_id}
		,data.since
		,data.current
		,m::vm::current_sequence
		,e.what()
	};
}
