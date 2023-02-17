// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2023 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

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

	const room::sounding s
	{
		room
	};

	s.rfor_each([&ret]
	(const auto &range, const auto &event_idx) noexcept
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

	const room::sounding s
	{
		room
	};

	s.rfor_each([&ret]
	(const auto &range, const auto &event_idx) noexcept
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

	const room::sounding s
	{
		room
	};

	s.for_each([&ret]
	(const auto &range, const auto &event_idx) noexcept
	{
		ret.first = range.first;
		return false;
	});

	return ret;
}

//
// room::sounding
//

bool
ircd::m::room::sounding::rfor_each(const closure &closure)
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
ircd::m::room::sounding::for_each(const closure &closure)
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
