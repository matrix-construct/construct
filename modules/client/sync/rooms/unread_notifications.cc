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
	"Client Sync :Room Unread Notifications"
};

namespace ircd::m::sync
{
	static long _notification_count(const room &, const event::idx &a, const event::idx &b);
	static long _highlight_count(const room &, const user &u, const event::idx &a, const event::idx &b);
	static bool room_unread_notifications_polylog(data &);
	static bool room_unread_notifications_linear(data &);
	extern item room_unread_notifications;
}

decltype(ircd::m::sync::room_unread_notifications)
ircd::m::sync::room_unread_notifications
{
	"rooms.unread_notifications",
	room_unread_notifications_polylog,
	room_unread_notifications_linear
};

bool
ircd::m::sync::room_unread_notifications_linear(data &data)
{
	return true;
}

bool
ircd::m::sync::room_unread_notifications_polylog(data &data)
{
	auto &room{*data.room};
	m::event::id::buf last_read;
	if(!m::receipt::read(last_read, room.room_id, data.user))
		return false;

	json::stack::object out
	{
		data.out
	};

	const auto last_read_idx
	{
		index(last_read)
	};

	// highlight_count
	json::stack::member
	{
		out, "highlight_count", json::value
		{
			_highlight_count(room, data.user, last_read_idx, data.current)
		}
	};

	// notification_count
	json::stack::member
	{
		out, "notification_count", json::value
		{
			_notification_count(room, last_read_idx, data.current)
		}
	};

	return true;
}

long
ircd::m::sync::_notification_count(const room &room,
                                   const event::idx &a,
                                   const event::idx &b)
{
	return m::count_since(room, a, a < b? b : a);
}

long
ircd::m::sync::_highlight_count(const room &r,
                                const user &u,
                                const event::idx &a,
                                const event::idx &b)
{
	using proto = size_t (const user &, const room &, const event::idx &, const event::idx &);

	static mods::import<proto> count
	{
		"m_user", "highlighted_count__between"
	};

	return count(u, r, a, a < b? b : a);
}
