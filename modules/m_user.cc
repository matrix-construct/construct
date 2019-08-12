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
	static room create_user_room(const user::id &, const room::id &, const json::members &contents);
}

ircd::mapi::header
IRCD_MODULE
{
	"Matrix user library; modular components."
};

ircd::m::user
IRCD_MODULE_EXPORT
ircd::m::create(const m::user::id &user_id,
                const json::members &contents)
{
	const m::user user
	{
		user_id
	};

	const m::room::id::buf room_id
	{
		user.room_id()
	};

	const m::room room
	{
		create_user_room(user_id, room_id, contents)
	};

	return user;
}

ircd::m::room
ircd::m::create_user_room(const user::id &user_id,
                          const room::id &room_id,
                          const json::members &contents)
try
{
	return create(room_id, m::me.user_id, "user");
}
catch(const std::exception &e)
{
	if(m::exists(room_id))
		return room_id;

	log::error
	{
		log, "Failed to create user %s room %s :%s",
		string_view{user_id},
		string_view{room_id},
		e.what()
	};

	throw;
}
