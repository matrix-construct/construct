// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

ircd::m::event::id::buf
ircd::m::user::pushrules::set(const path &path,
                              const json::object &value)
const
{
	const m::user::room user_room
	{
		user
	};

	return {};
}

void
ircd::m::user::pushrules::get(const path &path,
                              const closure &closure)
const
{
	if(!get(std::nothrow, path, closure))
		throw m::NOT_FOUND
		{
			"push rule (%s,%s,%s) for user %s not found",
			std::get<0>(path),
			std::get<1>(path),
			std::get<2>(path),
			string_view{user.user_id}
		};
}

bool
ircd::m::user::pushrules::get(std::nothrow_t,
                              const path &path,
                              const closure &closure)
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

	const event::idx &event_idx
	{
		0U
	};

	return false;
}

bool
ircd::m::user::pushrules::for_each(const closure_bool &closure)
const
{
	static const event::fetch::opts fopts
	{
		event::keys::include {"state_key", "content"}
	};

	const user::room user_room
	{
		user
	};

	const room::state state
	{
		user_room, &fopts
	};

	return true;
}
