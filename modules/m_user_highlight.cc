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
	"Matrix user library; highlight notification support"
};

decltype(ircd::m::user::highlight::enable_count)
ircd::m::user::highlight::enable_count
{
	{ "name",    "ircd.m.user.highlight.enable.count" },
	{ "default", true                                 },
};

decltype(ircd::m::user::highlight::match_mxid_full)
ircd::m::user::highlight::match_mxid_full
{
	{ "name",    "ircd.m.user.highlight.match.mxid.full" },
	{ "default", true                                    },
};

decltype(ircd::m::user::highlight::match_mxid_local_cs)
ircd::m::user::highlight::match_mxid_local_cs
{
	{ "name",    "ircd.m.user.highlight.match.mxid.local.cs" },
	{ "default", true                                        },
};

decltype(ircd::m::user::highlight::match_mxid_local_cs)
ircd::m::user::highlight::match_mxid_local_ci
{
	{ "name",    "ircd.m.user.highlight.match.mxid.local.ci" },
	{ "default", false                                       },
};

decltype(ircd::m::user::highlight::match_at_room)
ircd::m::user::highlight::match_at_room
{
	{ "name",    "ircd.m.user.highlight.match.at.room" },
	{ "default", true                                  },
};

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
	size_t ret(0);
	for_each(room, range, [this, &ret]
	(const m::event::idx &event_idx)
	{
		ret += has(event_idx);
		return true;
	});

	return ret;
}

bool
IRCD_MODULE_EXPORT
ircd::m::user::highlight::for_each(const m::room &room,
                                   const event::idx_range &range,
                                   const event::closure_idx_bool &closure)
const
{
	m::room::events it
	{
		room
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

	for(++it; it && it.event_idx() < range.second; ++it)
		if(!closure(it.event_idx()))
			return false;

	return true;
}

bool
IRCD_MODULE_EXPORT
ircd::m::user::highlight::has(const event::idx &event_idx)
const
{
	char typebuf[event::TYPE_MAX_SIZE];
	const string_view type
	{
		m::get(std::nothrow, event_idx, "type", typebuf)
	};

	bool ret{false};
	if(type != "m.room.message")
		return ret;

	m::get(std::nothrow, event_idx, "content", [this, &type, &ret]
	(const json::object &content)
	{
		m::event event;
		json::get<"content"_>(event) = content;
		json::get<"type"_>(event) = type;
		ret = has(event);
	});

	return ret;
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

	const json::string &body
	{
		content.get("body")
	};

	return match(body);
}

bool
IRCD_MODULE_EXPORT
ircd::m::user::highlight::match(const string_view &text)
const
{
	if(match_at_room)
		if(startswith(text, "@room"))
			return true;

	// Case insensitive and case-sensitive are exlusive; if both
	// are true only one branch is taken.
	if(match_mxid_local_ci)
	{
		if(imatch(text, user.user_id.localname()))
			return true;
	}
	else if(match_mxid_local_cs)
	{
		if(match(text, user.user_id.localname()))
			return true;
	}

	if(match_mxid_full)
		if(match(text, user.user_id))
			return true;

	return false;
}

namespace ircd::m
{
	static bool user_highlight_match(const string_view &text, const string_view &arg, const size_t &pos);
}

bool
IRCD_MODULE_EXPORT
ircd::m::user::highlight::match(const string_view &text,
                                const string_view &arg)
{
	const auto pos
	{
		text.find(arg)
	};

	return user_highlight_match(text, arg, pos);
}

bool
IRCD_MODULE_EXPORT
ircd::m::user::highlight::imatch(const string_view &text,
                                 const string_view &arg)
{
	const auto pos
	{
		ifind(text, arg)
	};

	return user_highlight_match(text, arg, pos);
}

bool
ircd::m::user_highlight_match(const string_view &text,
                              const string_view &arg,
                              const size_t &pos)
{
	static constexpr char sp {'\x20'}, colon {':'};

	// no match
	if(likely(pos == string_view::npos))
		return false;

	if(pos == 0)
	{
		// match is at the beginning of the string
		assert(pos + size(arg) <= size(text));
		if(pos + size(arg) == size(text))
			return true;

		return text[pos + size(arg)] == sp || text[pos + size(arg)] == colon;
	}

	if(pos + size(arg) == size(text))
	{
		// match is at the end of the string
		assert(size(arg) < size(text));
		return text[pos - 1] == sp;
	}

	// test if match surrounded by spaces
	assert(size(text) >= size(arg) + 2);
	return text[pos - 1] == sp && text[pos] == sp;
}
