// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

ircd::mapi::header
IRCD_MODULE
{
	"Matrix users interface"
};

void
IRCD_MODULE_EXPORT
ircd::m::users::for_each(const user::closure &closure)
{
	for_each(user::closure_bool{[&closure]
	(const m::user &user)
	{
		closure(user);
		return true;
	}});
}

bool
IRCD_MODULE_EXPORT
ircd::m::users::for_each(const user::closure_bool &closure)
{
	return for_each(string_view{}, closure);
}

bool
IRCD_MODULE_EXPORT
ircd::m::users::for_each(const string_view &lower_bound,
                         const user::closure_bool &closure)
{
	const m::room::state state
	{
		user::users
	};

	return state.for_each("ircd.user", lower_bound, m::room::state::keys_bool{[&closure]
	(const string_view &user_id)
	{
		const m::user &user
		{
			user_id
		};

		return closure(user);
	}});
}
