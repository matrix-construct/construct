// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include "account.h"

using namespace ircd;

extern "C" bool is_active__user(const m::user &user);
extern "C" m::event::id::buf activate__user(const m::user &user);
extern "C" m::event::id::buf deactivate__user(const m::user &user);

mapi::header
IRCD_MODULE
{
	"Client 3.4,3.5,3.6 :Account"
};

resource
account_resource
{
	"/_matrix/client/r0/account",
	{
		"(3.4,3.5,3.6) Account management"
	}
};

m::event::id::buf
activate__user(const m::user &user)
{
	const m::user::room user_room
	{
		user
	};

	return send(user_room, m::me.user_id, "ircd.account", "active", json::members
	{
		{ "value", true }
	});
}

bool
is_active__user(const m::user &user)
{
	const m::user::room user_room
	{
		user
	};

	bool ret{false};
	user_room.get(std::nothrow, "ircd.account", "active", [&ret]
	(const m::event &event)
	{
		const json::object &content
		{
			at<"content"_>(event)
		};

		ret = content.get<bool>("value") == true;
	});

	return ret;
}
