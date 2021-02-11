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

std::pair<int64_t, ircd::m::event::idx>
ircd::m::viewport(const room &room)
{
	std::pair<int64_t, m::event::idx> ret
	{
		-1, 0
	};

	m::room::events it
	{
		room
	};

	const ssize_t &max
	{
		room::events::viewport_size
	};

	for(auto i(0); it && i < max; --it, ++i)
	{
		ret.first = it.depth();
		ret.second = it.event_idx();
	}

	return ret;
}

std::pair<int64_t, ircd::m::event::idx>
ircd::m::twain(const room &room)
{
	std::pair<int64_t, m::event::idx> ret
	{
		-1, 0
	};

	const room::events::sounding s
	{
		room
	};

	s.rfor_each([&ret]
	(const auto &range, const auto &event_idx)
	{
		ret.first = range.first - 1;
		return false;
	});

	return ret;
}

std::pair<int64_t, ircd::m::event::idx>
ircd::m::sounding(const room &room)
{
	std::pair<int64_t, m::event::idx> ret
	{
		-1, 0
	};

	const room::events::sounding s
	{
		room
	};

	s.rfor_each([&ret]
	(const auto &range, const auto &event_idx)
	{
		ret.first = range.second;
		ret.second = event_idx;
		return false;
	});

	return ret;
}

std::pair<int64_t, ircd::m::event::idx>
ircd::m::hazard(const room &room)
{
	std::pair<int64_t, m::event::idx> ret
	{
		0, 0
	};

	const room::events::sounding s
	{
		room
	};

	s.for_each([&ret]
	(const auto &range, const auto &event_idx)
	{
		ret.first = range.first;
		return false;
	});

	return ret;
}

//
// room::events
//

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
                              const event::fetch::opts *const &fopts)
:room{room}
,_event
{
	fopts?
		*fopts:
	room.fopts?
		*room.fopts:
		event::fetch::default_opts
}
{
	assert(room.room_id);

	if(room.event_id)
		seek(room.event_id);
	else
		seek();
}

ircd::m::room::events::events(const m::room &room,
                              const event::id &event_id,
                              const event::fetch::opts *const &fopts)
:room{room}
,_event
{
	fopts?
		*fopts:
	room.fopts?
		*room.fopts:
		event::fetch::default_opts
}
{
	assert(room.room_id);

	seek(event_id);
}

ircd::m::room::events::events(const m::room &room,
                              const uint64_t &depth,
                              const event::fetch::opts *const &fopts)
:room{room}
,_event
{
	fopts?
		*fopts:
	room.fopts?
		*room.fopts:
		event::fetch::default_opts
}
{
	assert(room.room_id);

	// As a special convenience for the ctor only, if the depth=0 and
	// nothing is found another attempt is made for depth=1 for synapse
	// rooms which start at depth=1.
	if(!seek(depth) && depth == 0)
		seek(1);
}

bool
ircd::m::room::events::prefetch()
{
	assert(_event.fopts);
	return m::prefetch(event_idx(), *_event.fopts);
}

bool
ircd::m::room::events::prefetch(const string_view &event_prop)
{
	return m::prefetch(event_idx(), event_prop);
}

const ircd::m::event &
ircd::m::room::events::fetch()
{
	m::seek(_event, event_idx());
	return _event;
}

const ircd::m::event &
ircd::m::room::events::fetch(std::nothrow_t)
{
	m::seek(std::nothrow, _event, event_idx());
	return _event;
}

const ircd::m::event &
ircd::m::room::events::operator*()
{
	return fetch(std::nothrow);
};

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

ircd::m::room::events::operator
ircd::m::event::idx()
const
{
	return event_idx();
}

ircd::m::event::idx
ircd::m::room::events::event_idx()
const
{
	assert(bool(*this));
	const auto part
	{
		dbs::room_events_key(it->first)
	};

	return std::get<1>(part);
}

uint64_t
ircd::m::room::events::depth()
const
{
	assert(bool(*this));
	const auto part
	{
		dbs::room_events_key(it->first)
	};

	return std::get<0>(part);
}

//
// room::events::missing
//

size_t
ircd::m::room::events::missing::count()
const
{
	size_t ret{0};
	for_each([&ret]
	(const auto &event_id, const auto &depth, const auto &event_idx)
	{
		++ret;
		return true;
	});

	return ret;
}

bool
ircd::m::room::events::missing::for_each(const closure &closure)
const
{
	return for_each({0L, 0L}, closure);
}

bool
ircd::m::room::events::missing::for_each(const pair<int64_t> &depth,
                                         const closure &closure)
const
{
	room::events it
	{
		room, depth.first
	};

	bool ret{true};
	for(; it && ret; ++it)
	{
		if(depth.second && int64_t(it.depth()) > depth.second)
			break;

		const m::event &event{*it};
		const m::event::prev prev{event};
		ret = m::for_each(prev, [&](const m::event::id &event_id)
		{
			if(m::exists(event_id))
				return true;

			if(!closure(event_id, it.depth(), it.event_idx()))
				return false;

			return true;
		});
	}

	return ret;
}

bool
ircd::m::room::events::missing::rfor_each(const pair<int64_t> &depth,
                                          const closure &closure)
const
{
	room::events it
	{
		room, depth.second?: -1UL
	};

	bool ret{true};
	for(; it && ret; --it)
	{
		if(depth.second && int64_t(it.depth()) > depth.second)
			continue;

		if(int64_t(it.depth()) < depth.first)
			break;

		const m::event &event{*it};
		const m::event::prev prev{event};
		ret = m::for_each(prev, [&](const m::event::id &event_id)
		{
			if(m::exists(event_id))
				return true;

			if(!closure(event_id, it.depth(), it.event_idx()))
				return false;

			return true;
		});
	}

	return ret;
}

//
// room::events::sounding
//

bool
ircd::m::room::events::sounding::rfor_each(const closure &closure)
const
{
	const int64_t depth
	{
		room.event_id?
			m::get(std::nothrow, m::index(std::nothrow, room.event_id), "depth", -1L):
			-1L
	};

	room::events it
	{
		room, uint64_t(depth)
	};

	if(!it)
		return true;

	event::idx idx{0};
	for(sounding::range range{0L, it.depth()}; it; --it)
	{
		range.first = it.depth();
		if(range.first == range.second)
		{
			idx = it.event_idx();
			continue;
		}

		--range.second;
		if(range.first == range.second)
		{
			idx = it.event_idx();
			continue;
		}

		if(!closure({range.first+1, range.second+1}, idx))
			return false;

		range.second = range.first;
	}

	return true;
}

bool
ircd::m::room::events::sounding::for_each(const closure &closure)
const
{
	const int64_t depth
	{
		room.event_id?
			m::get(std::nothrow, m::index(std::nothrow, room.event_id), "depth", 0L):
			0L
	};

	room::events it
	{
		room, uint64_t(depth)
	};

	for(sounding::range range{depth, 0L}; it; ++it)
	{
		range.second = it.depth();
		if(range.first == range.second)
			continue;

		++range.first;
		if(range.first == range.second)
			continue;

		if(!closure(range, it.event_idx()))
			return false;

		range.first = range.second;
	}

	return true;
}

//
// room::events::horizon
//

//TODO: XXX remove fwd decl
namespace ircd::m::dbs
{
	void _index_event_horizon(db::txn &, const event &, const write_opts &, const m::event::id &);
}

size_t
ircd::m::room::events::horizon::rebuild()
{
	m::dbs::write_opts opts;
	opts.appendix.reset();
	opts.appendix.set(dbs::appendix::EVENT_HORIZON);
	db::txn txn
	{
		*dbs::events
	};

	size_t ret(0);
	m::room::events it
	{
		room
	};

	for(; it; --it)
	{
		const m::event &event{*it};
		const event::prev prev_events{event};

		opts.event_idx = it.event_idx();
		m::for_each(prev_events, [&]
		(const m::event::id &event_id)
		{
			if(m::exists(event_id))
				return true;

			m::dbs::_index_event_horizon(txn, event, opts, event_id);
			++ret;
			return true;
		});
	}

	txn();
	return ret;
}

size_t
ircd::m::room::events::horizon::count()
const
{
	size_t ret{0};
	for_each([&ret]
	(const auto &event_id, const auto &depth, const auto &event_idx)
	{
		++ret;
		return true;
	});

	return ret;
}

bool
ircd::m::room::events::horizon::for_each(const closure &closure)
const
{
	const std::function<bool (const string_view &)> in_room
	{
		[this](const string_view &room_id)
		{
			return room_id == this->room.room_id;
		}
	};

	return event::horizon::for_every([&in_room, &closure]
	(const event::id &event_id, const event::idx &event_idx)
	{
		if(!m::query(event_idx, "room_id", false, in_room))
			return true;

		if(m::exists(event_id))
			return true;

		uint64_t depth;
		if(!m::get(event_idx, "depth", depth))
			return true;

		if(!closure(event_id, depth, event_idx))
			return false;

		return true;
	});
}
