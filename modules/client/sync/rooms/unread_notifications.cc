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
	static void room_unread_notifications_polylog(data &);
	static void room_unread_notifications_linear(data &);
	extern item room_unread_notifications;
}

decltype(ircd::m::sync::room_unread_notifications)
ircd::m::sync::room_unread_notifications
{
	"rooms.unread_notifications",
	room_unread_notifications_polylog,
	room_unread_notifications_linear
};

void
ircd::m::sync::room_unread_notifications_linear(data &data)
{

}

void
ircd::m::sync::room_unread_notifications_polylog(data &data)
{
	const auto &room{*data.room};
	m::event::id::buf last_read;
	if(!m::receipt::read(last_read, room.room_id, data.user))
		return;

	const auto start_idx
	{
		index(last_read)
	};

	json::stack::object out
	{
		data.out
	};

	if(!apropos(data, start_idx))
		return;

	data.commit();

	// highlight_count
	json::stack::member
	{
		out, "highlight_count", json::value
		{
			_highlight_count(room, data.user, start_idx, data.range.second)
		}
	};

	// notification_count
	json::stack::member
	{
		out, "notification_count", json::value
		{
			_notification_count(room, start_idx, data.range.second)
		}
	};
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
