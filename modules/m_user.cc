// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

using namespace ircd::m;
using namespace ircd;

mapi::header
IRCD_MODULE
{
	"Matrix user library; modular components."
};

bool
IRCD_MODULE_EXPORT
ircd::m::device::set(const m::user &user,
                     const device &device)
{
	return true;
}

bool
IRCD_MODULE_EXPORT
ircd::m::device::del(const m::user &user,
                     const string_view &id)
{
	const m::user::room user_room{user};
	const m::room::state state{user_room};
	const m::event::idx event_idx
	{
		state.get(std::nothrow, "ircd.device", id)
	};

	if(!event_idx)
		return false;

	const m::event::id::buf event_id
	{
		m::event_id(event_idx, std::nothrow)
	};

	m::redact(user_room, user, event_id, "deleted");
	return true;
}

bool
IRCD_MODULE_EXPORT
ircd::m::device::get(std::nothrow_t,
                     const m::user &user,
                     const string_view &id,
                     const closure &closure)
{
	const m::user::room user_room{user};
	const m::room::state state{user_room};
	const m::event::idx event_idx
	{
		state.get(std::nothrow, "ircd.device", id)
	};

	if(!event_idx)
		return false;

	return m::get(std::nothrow, event_idx, "content", [&closure]
	(const json::object &content)
	{
		closure(content);
	});
}

bool
IRCD_MODULE_EXPORT
ircd::m::device::for_each(const m::user &user,
                          const closure_bool &closure)
{
	return for_each(user, id_closure_bool{[&user, &closure]
	(const string_view &device_id)
	{
		bool ret(true);
		get(std::nothrow, user, device_id, [&closure, &ret]
		(const device &device)
		{
			ret = closure(device);
		});

		return ret;
	}});
}

bool
IRCD_MODULE_EXPORT
ircd::m::device::for_each(const m::user &user,
                          const id_closure_bool &closure)
{
	const m::room::state::keys_bool state_key{[&closure]
	(const string_view &state_key)
	{
		return closure(state_key);
	}};

	const m::user::room user_room{user};
	const m::room::state state{user_room};
	return state.for_each("ircd.device", state_key);
}

extern "C" m::user
user_create(const m::user::id &user_id,
            const json::members &contents)
{
	const m::user user
	{
		user_id
	};

	const m::room::id::buf user_room_id
	{
		user.room_id()
	};

	//TODO: ABA
	//TODO: TXN
	m::room user_room
	{
		m::create(user_room_id, m::me.user_id, "user")
	};

	//TODO: ABA
	//TODO: TXN
	send(user.users, m::me.user_id, "ircd.user", user.user_id, contents);
	return user;
}

extern "C" bool
highlighted_event(const event &event,
                  const user &user)
{
	if(json::get<"type"_>(event) != "m.room.message")
		return false;

	const json::object &content
	{
		json::get<"content"_>(event)
	};

	const string_view &formatted_body
	{
		content.get("formatted_body")
	};

	if(has(formatted_body, user.user_id))
		return true;

	const string_view &body
	{
		content.get("body")
	};

	return has(body, user.user_id);
}

extern "C" size_t
highlighted_count__between(const user &user,
                           const room &room,
                           const event::idx &a,
                           const event::idx &b)
{
	static const event::fetch::opts fopts
	{
		event::keys::include
		{
			"type", "content",
		}
	};

	room::messages it
	{
		room, &fopts
	};

	assert(a <= b);
	it.seek_idx(a);

	if(!it && !exists(room))
		throw NOT_FOUND
		{
			"Cannot find room '%s' to count highlights for '%s'",
			string_view{room.room_id},
			string_view{user.user_id}
		};
	else if(!it)
		throw NOT_FOUND
		{
			"Event @ idx:%lu or idx:%lu not found in '%s' to count highlights for '%s'",
			a,
			b,
			string_view{room.room_id},
			string_view{user.user_id},
		};

	size_t ret{0};
	for(++it; it && it.event_idx() <= b; ++it)
	{
		const event &event{*it};
		ret += highlighted_event(event, user);
	}

	return ret;
}

extern "C" size_t
highlighted_count__since(const user &user,
                         const room &room,
                         const event::idx &current)
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

	return highlighted_count__between(user, room, a, current);
}

extern "C" size_t
highlighted_count(const user &user,
                  const room &room)
{
	const auto &current
	{
		head_idx(room)
	};

	return highlighted_count__since(user, room, current);
}
