// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

bool
ircd::m::user::pushers::del(const string_view &key)
const
{
	const m::user::room user_room
	{
		user
	};

	static const auto &type
	{
		push::pusher::type_prefix
	};

	const event::idx &event_idx
	{
		user_room.get(type, key)
	};

	const m::event::id::buf event_id
	{
		m::event_id(std::nothrow, event_idx)
	};

	if(!event_id)
		return false;

	const auto redact_id
	{
		m::redact(user_room, user, event_id, "deleted")
	};

	return true;
}

bool
ircd::m::user::pushers::set(const json::object &content)
const
{
	const json::string &key
	{
		content.at("pushkey")
	};

	const json::string &kind
	{
		content.at("kind")
	};

	if(kind == "null")
		return del(key);

	const m::user::room user_room
	{
		user
	};

	static const auto &type
	{
		push::pusher::type_prefix
	};

	const auto pusher_event_id
	{
		m::send(user_room, user, type, key, content)
	};

	return true;
}

void
ircd::m::user::pushers::get(const string_view &key,
                            const closure &closure)
const
{
	if(!get(std::nothrow, key, closure))
		throw m::NOT_FOUND
		{
			"pusher '%s' for user %s not found",
			key,
			string_view{user.user_id}
		};
}

bool
ircd::m::user::pushers::get(std::nothrow_t,
                            const string_view &key,
                            const closure &closure)
const
{
	const m::user::room user_room
	{
		user
	};

	static const auto &type
	{
		push::pusher::type_prefix
	};

	const event::idx &event_idx
	{
		user_room.get(type, key)
	};

	return m::get(std::nothrow, event_idx, "content", [&key, &closure, &event_idx]
	(const json::object &content)
	{
		closure(event_idx, key, content);
	});
}

bool
ircd::m::user::pushers::has(const string_view &key)
const
{
	return !for_each([&key] // for_each() returns true if no match
	(const event::idx &pusher_idx, const string_view &pushkey, const push::pusher &pusher)
	{
		return key != pushkey;
	});
}

bool
ircd::m::user::pushers::any(const string_view &kind)
const
{
	return !for_each([&kind] // for_each() returns true if no match
	(const event::idx &pusher_idx, const string_view &pushkey, const push::pusher &pusher)
	{
		if(!kind)
			return false;

		if(json::get<"kind"_>(pusher) == kind)
			return false;

		return true;
	});
}

size_t
ircd::m::user::pushers::count(const string_view &kind)
const
{
	size_t ret{0};
	for_each([&ret, &kind]
	(const event::idx &pusher_idx, const string_view &pushkey, const push::pusher &pusher)
	{
		ret += !kind || json::get<"kind"_>(pusher) == kind;
		return true;
	});

	return ret;
}

bool
ircd::m::user::pushers::for_each(const closure_bool &closure)
const
{
	const user::room user_room
	{
		user
	};

	const room::state state
	{
		user_room
	};

	static const auto &type
	{
		push::pusher::type_prefix
	};

	return state.for_each(type, room::state::closure_bool{[&closure]
	(const string_view &_type, const string_view &state_key, const m::event::idx &event_idx)
	{
		assert(type == _type);
		return m::query<bool>(std::nothrow, event_idx, "content", true, [&]
		(const json::object &content)
		{
			return closure(event_idx, state_key, content);
		});
	}});
}
