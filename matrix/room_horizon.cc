// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2023 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

//TODO: XXX remove fwd decl
namespace ircd::m::dbs
{
	void _index_event_horizon(db::txn &, const event &, const opts &, const m::event::id &);
}

size_t
ircd::m::room::horizon::rebuild()
{
	m::dbs::opts opts;
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

	m::event::fetch event;
	for(; it; --it)
	{
		if(!seek(std::nothrow, event, it.event_idx()))
			continue;

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
ircd::m::room::horizon::count()
const
{
	size_t ret{0};
	for_each([&ret]
	(const auto &event_id, const auto &depth, const auto &event_idx) noexcept
	{
		++ret;
		return true;
	});

	return ret;
}

bool
ircd::m::room::horizon::for_each(const closure &closure)
const
{
	const std::function<bool (const string_view &)> in_room
	{
		[this](const string_view &room_id) noexcept
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
