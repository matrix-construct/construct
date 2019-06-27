// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

using namespace ircd::m;
using namespace ircd;

mapi::header
IRCD_MODULE
{
	"Matrix user library; modular components."
};

extern "C" m::user
user_create(const m::user::id &user_id,
            const json::members &contents)
{
	const m::user user
	{
		user_id
	};

	const m::room::id::buf user_room_id
	{
		user.room_id()
	};

	//TODO: ABA
	//TODO: TXN
	m::room user_room
	{
		m::create(user_room_id, m::me.user_id, "user")
	};

	//TODO: ABA
	//TODO: TXN
	send(user.users, m::me.user_id, "ircd.user", user.user_id, contents);
	return user;
}

///////////////////////////////////////////////////////////////////////////////
//
// m/user/highlight.h
//

size_t
IRCD_MODULE_EXPORT
ircd::m::user::highlight::count()
const
{
	const m::user::rooms rooms
	{
		user
	};

	size_t ret(0);
	rooms.for_each("join", m::user::rooms::closure_bool{[this, &ret]
	(const m::room &room, const string_view &)
	{
		ret += this->count(room);
		return true;
	}});

	return ret;
}

size_t
IRCD_MODULE_EXPORT
ircd::m::user::highlight::count(const m::room &room)
const
{
	const auto &current
	{
		head_idx(room)
	};

	return count_to(room, current);
}

size_t
IRCD_MODULE_EXPORT
ircd::m::user::highlight::count_to(const m::room &room,
                                   const event::idx &current)
const
{
	event::id::buf last_read_buf;
	const event::id last_read
	{
		receipt::read(last_read_buf, room, user)
	};

	if(!last_read)
		return 0;

	const auto &a
	{
		index(last_read)
	};

	const event::idx_range range
	{
		a, current
	};

	return count_between(room, range);
}

size_t
IRCD_MODULE_EXPORT
ircd::m::user::highlight::count_between(const m::room &room,
                                        const event::idx_range &range)
const
{
	static const event::fetch::opts fopts
	{
		event::keys::include { "type", "content" },
	};

	m::room::messages it
	{
		room, &fopts
	};

	assert(range.first <= range.second);
	it.seek_idx(range.first);

	if(!it && !exists(room))
		throw m::NOT_FOUND
		{
			"Cannot find room '%s' to count highlights for '%s'",
			string_view{room.room_id},
			string_view{user.user_id}
		};
	else if(!it)
		throw m::NOT_FOUND
		{
			"Event @ idx:%lu or idx:%lu not found in '%s' to count highlights for '%s'",
			range.first,
			range.second,
			string_view{room.room_id},
			string_view{user.user_id},
		};

	size_t ret{0};
	for(++it; it && it.event_idx() < b; ++it)
	{
		const event &event{*it};
		ret += has(event);
	}

	return ret;
}

bool
IRCD_MODULE_EXPORT
ircd::m::user::highlight::has(const event::idx &event_idx)
const
{
	const event::fetch event
	{
		event_idx, std::nothrow
	};

	return event.valid?
		has(event):
		false;
}

bool
IRCD_MODULE_EXPORT
ircd::m::user::highlight::has(const event &event)
const
{
	if(json::get<"type"_>(event) != "m.room.message")
		return false;

	const json::object &content
	{
		json::get<"content"_>(event)
	};

	const string_view &formatted_body
	{
		content.get("formatted_body")
	};

	if(ircd::has(formatted_body, user.user_id))
		return true;

	const string_view &body
	{
		content.get("body")
	};

	return ircd::has(body, user.user_id);
}
