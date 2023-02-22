// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2023 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

size_t
ircd::m::room::missing::count()
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
ircd::m::room::missing::for_each(const closure &closure)
const
{
	return for_each({0L, 0L}, closure);
}

bool
ircd::m::room::missing::for_each(const pair<int64_t> &depth,
                                 const closure &closure)
const
{
	room::events it
	{
		room, uint64_t(depth.first)
	};

	m::event::fetch event;
	for(; it; ++it)
	{
		if(depth.second && int64_t(it.depth()) > depth.second)
			break;

		if(!_each(it, event, closure))
			return false;
	}

	return true;
}

bool
ircd::m::room::missing::rfor_each(const pair<int64_t> &depth,
                                  const closure &closure)
const
{
	room::events it
	{
		room, depth.second?: -1UL
	};

	m::event::fetch event;
	for(; it; --it)
	{
		if(depth.second && int64_t(it.depth()) > depth.second)
			continue;

		if(int64_t(it.depth()) < depth.first)
			break;

		if(!_each(it, event, closure))
			return false;
	}

	return true;
}

bool
ircd::m::room::missing::_each(m::room::events &it,
                              m::event::fetch &event,
                              const closure &closure)
const
{
	const auto &[depth, event_idx]
	{
		*it
	};

	if(!seek(std::nothrow, event, event_idx))
		return true;

	const event::prev prev
	{
		event
	};

	event::idx idx_buf[event::prev::MAX];
	const auto prev_idx
	{
		prev.idxs(idx_buf)
	};

	for(size_t i(0); i < prev_idx.size(); ++i)
	{
		if(prev_idx[i])
			continue;

		if(!closure(prev.prev_event(i), depth, event_idx))
			return false;
	}

	return true;
}
