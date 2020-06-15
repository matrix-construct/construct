// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

bool
ircd::m::room::members::empty()
const
{
	return empty(string_view{}, string_view{});
}

bool
ircd::m::room::members::empty(const string_view &membership)
const
{
	return empty(membership, string_view{});
}

bool
ircd::m::room::members::empty(const string_view &membership,
                              const string_view &host)
const
{
	return for_each(membership, host, closure{[]
	(const user::id &user_id)
	{
		return false;
	}});
}

size_t
ircd::m::room::members::count()
const
{
	return count(string_view{}, string_view{});
}

size_t
ircd::m::room::members::count(const string_view &membership)
const
{
	return count(membership, string_view{});
}

size_t
ircd::m::room::members::count(const string_view &membership,
                              const string_view &host)
const
{
	size_t ret{0};
	for_each(membership, host, closure{[&ret]
	(const user::id &user_id)
	{
		++ret;
		return true;
	}});

	return ret;
}

bool
ircd::m::room::members::for_each(const closure &closure)
const
{
	return for_each(string_view{}, closure);
}

bool
ircd::m::room::members::for_each(const closure_idx &closure)
const
{
	return for_each(string_view{}, closure);
}

bool
ircd::m::room::members::for_each(const string_view &membership,
                                 const closure &closure)
const
{
	return for_each(membership, string_view{}, closure);
}

bool
ircd::m::room::members::for_each(const string_view &membership,
                                 const closure_idx &closure)
const
{
	return for_each(membership, string_view{}, closure);
}

bool
ircd::m::room::members::for_each(const string_view &membership,
                                 const string_view &host,
                                 const closure &closure)
const
{
	const m::room::state state
	{
		room
	};

	const bool present
	{
		state.present()
	};

	const closure_idx _closure
	{
		[&closure](const auto &user_id, const auto &event_idx)
		{
			return closure(user_id);
		}
	};

	// joined members optimization. Only possible when seeking
	// membership="join" on the present state of the room.
	if(membership == "join" && present)
		return for_each_join_present(host, _closure);

	return this->for_each(membership, host, _closure);
}

bool
ircd::m::room::members::for_each(const string_view &membership,
                                 const string_view &host,
                                 const closure_idx &closure)
const
{
	const m::room::state state
	{
		room
	};

	const bool present
	{
		state.present()
	};

	// joined members optimization. Only possible when seeking
	// membership="join" on the present state of the room.
	if(membership == "join" && present)
		return for_each_join_present(host, [&closure, &state]
		(const id::user &user_id, event::idx event_idx)
		{
			if(!event_idx)
				event_idx = state.get(std::nothrow, "m.room.member", user_id);

			if(unlikely(!event_idx))
			{
				log::error
				{
					log, "Failed member:%s event_idx:%lu in room_joined of %s",
					string_view{user_id},
					event_idx,
					string_view{state.room_id},
				};

				return true;
			}

			return closure(user_id, event_idx);
		});

	return state.for_each("m.room.member", [this, &host, &membership, &closure]
	(const string_view &type, const string_view &state_key, const event::idx &event_idx)
	{
		const m::user::id &user_id
		{
			state_key
		};

		if(host && user_id.host() != host)
			return true;

		return !membership || m::membership(event_idx, membership)?
			closure(user_id, event_idx):
			true;
	});
}

bool
ircd::m::room::members::for_each_join_present(const string_view &host,
                                              const closure_idx &closure)
const
{
	db::domain &index
	{
		dbs::room_joined
	};

	char keybuf[dbs::ROOM_JOINED_KEY_MAX_SIZE];
	const string_view &key
	{
		dbs::room_joined_key(keybuf, room.room_id, host)
	};

	auto it
	{
		index.begin(key)
	};

	for(; bool(it); ++it)
	{
		const auto &[origin, user_id]
		{
			dbs::room_joined_key(it->first)
		};

		if(host && origin != host)
			break;

		const event::idx event_idx
		{
			it->second.size() >= sizeof(event::idx)?
				event::idx(byte_view<event::idx>(it->second)):
				0UL
		};

		if(!closure(user_id, event_idx))
			return false;
	}

	return true;
}
