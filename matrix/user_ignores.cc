// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m
{
	extern conf::item<bool> ignored_user_list_enforce_invites;
	extern conf::item<bool> ignored_user_list_enforce_events;
}

decltype(ircd::m::ignored_user_list_enforce_invites)
ircd::m::ignored_user_list_enforce_invites
{
	{ "name",     "ircd.m.ignored_user_list.enforce.invites" },
	{ "default",  true                                       }
};

decltype(ircd::m::ignored_user_list_enforce_events)
ircd::m::ignored_user_list_enforce_events
{
	{ "name",     "ircd.m.ignored_user_list.enforce.events" },
	{ "default",  false                                     }
};

bool
ircd::m::user::ignores::has(const m::user::id &other)
const
{
	return !for_each([&other]
	(const m::user::id &user_id, const json::object &)
	{
		return user_id != other;
	});
}

bool
ircd::m::user::ignores::for_each(const closure_bool &closure)
const try
{
	const m::user::account_data account_data
	{
		user
	};

	bool ret{true};
	account_data.get(std::nothrow, "m.ignored_user_list", [&closure, &ret]
	(const string_view &key, const json::object &content)
	{
		const json::object &ignored_users
		{
			content.get("ignored_users")
		};

		for(const auto &[user_id, object] : ignored_users)
			if(!(ret = closure(user_id, object)))
				return;
	});

	return ret;
}
catch(const std::exception &e)
{
	log::derror
	{
		m::log, "Error in ignore list for %s",
		string_view{user.user_id}
	};

	return true;
}

bool
ircd::m::user::ignores::enforce(const string_view &type)
{
	if(type == "events")
		return bool(ignored_user_list_enforce_events);

	if(type == "invites")
		return bool(ignored_user_list_enforce_invites);

	return false;
}
