// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

decltype(ircd::m::room::events::viewport_size)
ircd::m::room::events::viewport_size
{
	{ "name",     "ircd.m.room.events.viewport.size" },
	{ "default",  96L                                },
};

size_t
ircd::m::room::events::count(const m::event::idx_range &range)
{
	const auto &[a, b]
	{
		range
	};

	room::id::buf room_id
	{
		m::get(std::min(a, b), "room_id", room_id)
	};

	return count(room_id, range);
}

size_t
ircd::m::room::events::count(const m::room &room,
                             const m::event::idx_range &range)
{
	const auto &[a, b]
	{
		range
	};

	assert(a <= b);
	m::room::events it
	{
		room, event::id{}
	};

	size_t ret(0);
	if(a == b)
		return ret;

	if(unlikely(!it.seek_idx(b, true)))
		return ret;

	for(assert(it); it && it.event_idx() > a; --it)
		++ret;

	return ret;
}

size_t
ircd::m::room::events::prefetch_viewport(const m::room &room)
{
	m::room::events it
	{
		room
	};

	const event::fetch::opts &fopts
	{
		room.fopts?
			*room.fopts:
			event::fetch::default_opts
	};

	ssize_t i(0), ret(0);
	for(; it && i < viewport_size; --it, ++i)
	{
		const auto &event_idx(it.event_idx());
		ret += m::prefetch(event_idx, fopts);
	}

	return ret;
}

size_t
ircd::m::room::events::prefetch(const m::room &room,
                                const depth_range &range)
{
	m::room::events it
	{
		room, std::max(range.first, range.second)
	};

	const event::fetch::opts &fopts
	{
		room.fopts?
			*room.fopts:
			event::fetch::default_opts
	};

	ssize_t i(0), ret(0);
	for(; it && i < viewport_size; --it, ++i)
	{
		const auto &depth(it.depth());
		const auto &event_idx(it.event_idx());
		ret += m::prefetch(event_idx, fopts);
		if(depth <= std::min(range.first, range.second))
			break;
	}

	return ret;
}

bool
ircd::m::room::events::preseek(const m::room &room,
                               const uint64_t &depth)
{
	char buf[dbs::ROOM_EVENTS_KEY_MAX_SIZE];
	const string_view key
	{
		depth != uint64_t(-1)?
			dbs::room_events_key(buf, room.room_id, depth):
			string_view{room.room_id}
	};

	return db::prefetch(dbs::room_events, key);
}

//
// room::events::events
//

ircd::m::room::events::events(const m::room &room,
                              const event::fetch::opts *const fopts)
:room{room}
{
	assert(room.room_id);

	if(fopts)
		this->room.fopts = fopts;

	const bool found
	{
		room.event_id?
			seek(room.event_id):
			seek()
	};

	assert(found == bool(*this));
}

ircd::m::room::events::events(const m::room &room,
                              const event::id &event_id,
                              const event::fetch::opts *const fopts)
:room{room}
{
	assert(room.room_id);

	if(fopts)
		this->room.fopts = fopts;

	const bool found
	{
		seek(event_id)
	};

	assert(found == bool(*this));
}

ircd::m::room::events::events(const m::room &room,
                              const uint64_t &depth,
                              const event::fetch::opts *const fopts)
:room{room}
{
	assert(room.room_id);

	if(fopts)
		this->room.fopts = fopts;

	// As a special convenience for the ctor only, if the depth=0 and
	// nothing is found another attempt is made for depth=1 for synapse
	// rooms which start at depth=1.
	if(!seek(depth) && depth == 0)
		seek(1);
}

bool
ircd::m::room::events::prefetch()
{
	const auto &fopts
	{
		room.fopts?
			*room.fopts:
			event::fetch::default_opts
	};

	return m::prefetch(event_idx(), fopts);
}

bool
ircd::m::room::events::prefetch(const string_view &event_prop)
{
	return m::prefetch(event_idx(), event_prop);
}

bool
ircd::m::room::events::preseek(const uint64_t &depth)
{
	char buf[dbs::ROOM_EVENTS_KEY_MAX_SIZE];
	const string_view key
	{
		depth != uint64_t(-1)?
			dbs::room_events_key(buf, room.room_id, depth):
			room.room_id
	};

	return db::prefetch(dbs::room_events, key);
}

bool
ircd::m::room::events::seek(const event::id &event_id)
{
	const event::idx &event_idx
	{
		m::index(std::nothrow, event_id)
	};

	return seek_idx(event_idx);
}

bool
ircd::m::room::events::seek(const uint64_t &depth)
{
	char buf[dbs::ROOM_EVENTS_KEY_MAX_SIZE];
	const string_view seek_key
	{
		depth != uint64_t(-1)?
			dbs::room_events_key(buf, room.room_id, depth):
			room.room_id
	};

	this->it = dbs::room_events.begin(seek_key);
	return bool(*this);
}

bool
ircd::m::room::events::seek_idx(const event::idx &event_idx,
                                const bool &lower_bound)
{
	if(!event_idx)
		return false;

	const uint64_t depth
	{
		m::get(std::nothrow, event_idx, "depth", -1UL)
	};

	char buf[dbs::ROOM_EVENTS_KEY_MAX_SIZE];
	const auto &seek_key
	{
		dbs::room_events_key(buf, room.room_id, depth, event_idx)
	};

	this->it = dbs::room_events.begin(seek_key);
	if(!bool(*this))
		return false;

	// When lower_bound=false we need to find the exact event being sought
	if(!lower_bound && event_idx != this->event_idx())
		return false;

	return true;
}

const ircd::m::room::events::entry *
ircd::m::room::events::operator->()
const
{
	assert(bool(*this));
	_entry = dbs::room_events_key(it->first);
	return &_entry;
}
