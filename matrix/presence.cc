// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m
{
	extern const string_view presence_valid_states[];
}

decltype(ircd::m::presence_valid_states)
ircd::m::presence_valid_states
{
	"online",
	"offline",
	"unavailable",
};

ircd::m::presence::presence(const user &user,
                            const mutable_buffer &buf)
:edu::m_presence{[&user, &buf]
{
	json::object ret;
	get(user, [&ret, &buf]
	(const json::object &content)
	{
		ret =
		{
			data(buf), copy(buf, string_view{content})
		};
	});

	return ret;
}()}
{
}

ircd::m::event::id::buf
ircd::m::presence::set(const user &user,
                       const string_view &presence,
                       const string_view &status_msg)
{
	return set(m::presence
	{
		{ "user_id",           user.user_id         },
		{ "presence",          presence             },
		{ "status_msg",        status_msg           },
		{ "currently_active",  presence == "online" },
	});
}

void
ircd::m::presence::get(const user &user,
                       const closure &closure)
{
	if(!get(std::nothrow, user, closure))
		throw m::NOT_FOUND
		{
			"No presence found for %s", string_view{user.user_id}
		};
}

bool
ircd::m::presence::get(std::nothrow_t,
                       const user &user,
                       const closure &closure)
{
	static const m::event::fetch::opts fopts
	{
		m::event::keys::include {"content"}
	};

	const auto reclosure{[&closure]
	(const m::event &event)
	{
		closure(json::get<"content"_>(event));
	}};

	return get(std::nothrow, user, reclosure, &fopts);
}

ircd::m::event::idx
ircd::m::presence::get(const user &user)
{
	const event::idx ret
	{
		get(std::nothrow, user)
	};

	if(!ret)
		throw m::NOT_FOUND
		{
			"No presence found for %s", string_view{user.user_id}
		};

	return ret;
}

bool
ircd::m::presence::get(const std::nothrow_t,
                       const m::user &user,
                       const m::presence::closure_event &closure,
                       const m::event::fetch::opts *const &fopts_p)
{
	const m::event::idx event_idx
	{
		m::presence::get(std::nothrow, user)
	};

	if(!event_idx)
		return false;

	const auto &fopts
	{
		fopts_p? *fopts_p : event::fetch::default_opts
	};

	const m::event::fetch event
	{
		std::nothrow, event_idx, fopts
	};

	if(event.valid)
		closure(event);

	return event.valid;
}

ircd::m::event::idx
ircd::m::presence::get(const std::nothrow_t,
                       const m::user &user)
{
	const m::user::room user_room
	{
		user
	};

	const m::room::state state
	{
		user_room
	};

	return state.get(std::nothrow, "ircd.presence", "");
}

bool
ircd::m::presence::prefetch(const m::user &user)
{
	const m::user::room user_room
	{
		user
	};

	const m::room::state state
	{
		user_room
	};

	return state.prefetch("ircd.presence", "");
}

ircd::m::event::id::buf
ircd::m::presence::set(const m::presence &content)
{
	const m::user user
	{
		json::at<"user_id"_>(content)
	};

	//TODO: ABA
	if(!exists(user))
		create(user.user_id);

	m::vm::copts copts;
	const m::user::room user_room
	{
		user, &copts
	};

	const json::strung _content
	{
		content
	};

	return send(user_room, user.user_id, "ircd.presence", "", json::object(_content));
}

bool
ircd::m::presence::valid_state(const string_view &state)
{
	return std::any_of(begin(presence_valid_states), end(presence_valid_states), [&state]
	(const string_view &valid)
	{
		return state == valid;
	});
}
