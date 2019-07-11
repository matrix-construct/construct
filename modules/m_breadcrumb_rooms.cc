// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

///////////////////////////////////////////////////////////////////////////////
//
// m/breadcrumb_rooms.h
//

ircd::m::event::id::buf
ircd::m::breadcrumb_rooms::set(const json::array &rooms)
const
{
	const json::strung object
	{
		json::members
		{
			{ "rooms", rooms }
		}
	};

	return account_data.set("im.vector.riot.breadcrumb_rooms", object);
}

bool
ircd::m::breadcrumb_rooms::for_each(const closure_bool &closure)
const
{
	bool ret{true};
	get(std::nothrow, [&closure, &ret]
	(const json::array &rooms)
	{
		for(const json::string &room : rooms)
			if(!closure(room))
			{
				ret = false;
				break;
			}
	});

	return ret;
}

void
ircd::m::breadcrumb_rooms::get(const closure &closure)
const
{
	if(!get(std::nothrow, closure))
		throw m::NOT_FOUND
		{
			"User has no breadcrumb_rooms set in their account_data."
		};
}

bool
ircd::m::breadcrumb_rooms::get(std::nothrow_t,
                               const closure &closure)
const
{
	return account_data.get(std::nothrow, "im.vector.riot.breadcrumb_rooms", [&closure]
	(const string_view &key, const json::object &object)
	{
		const json::array &rooms
		{
			object["rooms"]
		};

		closure(rooms);
	});
}
