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
ircd::m::breadcrumbs::set(const json::array &rooms)
const
{
	const json::strung object
	{
		json::members
		{
			{ "recent_rooms", rooms }
		}
	};

	return account_data.set("im.vector.setting.breadcrumbs", object);
}

bool
ircd::m::breadcrumbs::for_each(const closure_bool &closure)
const
{
	bool ret{true};
	get(std::nothrow, [&closure, &ret]
	(const json::array &rooms)
	{
		for(const json::string room_id : rooms)
			if(!closure(room_id))
			{
				ret = false;
				break;
			}
	});

	return ret;
}

void
ircd::m::breadcrumbs::get(const closure &closure)
const
{
	if(!get(std::nothrow, closure))
		throw m::NOT_FOUND
		{
			"User has no breadcrumbs set in their account_data."
		};
}

bool
ircd::m::breadcrumbs::get(std::nothrow_t,
                          const closure &closure)
const
{
	return account_data.get(std::nothrow, "im.vector.setting.breadcrumbs", [&closure]
	(const string_view &key, const json::object &object)
	{
		const json::array &rooms
		{
			object["recent_rooms"]
		};

		closure(rooms);
	});
}
