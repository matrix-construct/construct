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

bool
IRCD_MODULE_EXPORT
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
		event_idx, std::nothrow, fopts
	};

	if(event.valid)
		closure(event);

	return event.valid;
}

ircd::m::event::idx
IRCD_MODULE_EXPORT
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

ircd::m::event::id::buf
IRCD_MODULE_EXPORT
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

	//TODO: ABA
	return send(user_room, user.user_id, "ircd.presence", "", json::strung{content});
}

bool
IRCD_MODULE_EXPORT
ircd::m::presence::valid_state(const string_view &state)
{
	return std::any_of(begin(presence_valid_states), end(presence_valid_states), [&state]
	(const string_view &valid)
	{
		return state == valid;
	});
}
