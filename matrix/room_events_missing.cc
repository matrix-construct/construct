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
ircd::m::room::events::missing::count()
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
		room, uint64_t(depth.first)
	};

	for(; it; ++it)
	{
		if(depth.second && int64_t(it.depth()) > depth.second)
			break;

		if(!_each(it, closure))
			return false;
	}

	return true;
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

	for(; it; --it)
	{
		if(depth.second && int64_t(it.depth()) > depth.second)
			continue;

		if(int64_t(it.depth()) < depth.first)
			break;

		if(!_each(it, closure))
			return false;
	}

	return true;
}

bool
ircd::m::room::events::missing::_each(m::room::events &it,
                                      const closure &closure)
const
{
	const m::event event
	{
		*it
	};

	const event::prev prev
	{
		event
	};

	event::idx idx_buf[event::prev::MAX];
	const auto idx
	{
		prev.idxs(idx_buf)
	};

	for(size_t i(0); i < idx.size(); ++i)
	{
		if(idx[i])
			continue;

		if(!closure(prev.prev_event(i), it.depth(), it.event_idx()))
			return false;
	}

	return true;
}
