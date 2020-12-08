// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.


//
// room::state::history
//

ircd::m::room::state::history::history(const m::room &room)
:history
{
	room, -1
}
{
}

ircd::m::room::state::history::history(const m::room::id &room_id,
                                       const m::event::id &event_id)
:history
{
	m::room
	{
		room_id, event_id
	}
}
{
}

ircd::m::room::state::history::history(const m::room &room,
                                       const int64_t &bound)
:space
{
	room
}
,event_idx
{
	room.event_id?
		m::index(room.event_id):
		0UL
}
,bound
{
	bound < 0 && room.event_id?
		m::get<int64_t>(event_idx, "depth"):
		bound
}
{
}

bool
ircd::m::room::state::history::prefetch(const string_view &type)
const
{
	return prefetch(type, string_view{});
}

bool
ircd::m::room::state::history::prefetch(const string_view &type,
                                        const string_view &state_key)
const
{
	return space.prefetch(type, state_key, bound);
}

ircd::m::event::idx
ircd::m::room::state::history::get(const string_view &type,
                                   const string_view &state_key)
const
{
	const auto ret
	{
		get(std::nothrow, type, state_key)
	};

	if(unlikely(!ret))
		throw m::NOT_FOUND
		{
			"(%s,%s) in %s @%ld$%s",
			type,
			state_key,
			string_view{space.room.room_id},
			bound,
			string_view{space.room.event_id},
		};

	return ret;
}

ircd::m::event::idx
ircd::m::room::state::history::get(std::nothrow_t,
                                   const string_view &type,
                                   const string_view &state_key)
const
{
	event::idx ret{0};
	assert(type && defined(state_key));
	for_each(type, state_key, [&ret]
	(const auto &, const auto &, const auto &, const auto &event_idx)
	{
		ret = event_idx;
		return false;
	});

	return ret;
}

bool
ircd::m::room::state::history::has(const string_view &type)
const
{
	return has(type, string_view{});
}

bool
ircd::m::room::state::history::has(const string_view &type,
                                   const string_view &state_key)
const
{
	return !for_each(type, state_key, []
	(const auto &type, const auto &state_key, const auto &depth, const auto &event_idx)
	{
		return false;
	});
}

size_t
ircd::m::room::state::history::count(const string_view &type)
const
{
	return count(type, string_view{});
}

size_t
ircd::m::room::state::history::count(const string_view &type,
                                     const string_view &state_key)
const
{
	size_t ret(0);
	for_each(type, state_key, [&ret]
	(const auto &type, const auto &state_key, const auto &depth, const auto &event_idx)
	{
		++ret;
		return true;
	});

	return ret;
}

bool
ircd::m::room::state::history::for_each(const closure &closure)
const
{
	return for_each(string_view{}, string_view{}, closure);
}

bool
ircd::m::room::state::history::for_each(const string_view &type,
                                        const closure &closure)
const
{
	return for_each(type, string_view{}, closure);
}

bool
ircd::m::room::state::history::for_each(const string_view &type,
                                        const string_view &state_key,
                                        const closure &closure)
const
{
	char type_buf[m::event::TYPE_MAX_SIZE];
	char state_key_buf[m::event::STATE_KEY_MAX_SIZE];

	string_view last_type;
	string_view last_state_key;

	return space.for_each(type, state_key, [&]
	(const auto &type, const auto &state_key, const auto &depth, const auto &event_idx)
	{
		if(bound > -1 && depth >= bound && event_idx != this->event_idx)
			return true;

		if(type == last_type && state_key == last_state_key)
			return true;

		if(!closure(type, state_key, depth, event_idx))
			return false;

		if(type != last_type)
			last_type = { type_buf, copy(type_buf, type) };

		if(state_key != last_state_key)
			last_state_key = { state_key_buf, copy(state_key_buf, state_key) };

		return true;
	});
}
