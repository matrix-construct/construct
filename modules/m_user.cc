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
