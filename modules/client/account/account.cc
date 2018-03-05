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

extern "C" bool
is_active__user(const m::user &user)
{
	bool ret{false};
	const m::room &users{m::user::users};
	users.get(std::nothrow, "ircd.user", user.user_id, [&ret]
	(const m::event &event)
	{
		const json::object &content
		{
			at<"content"_>(event)
		};

		ret = content.get("active") == "true";
	});

	return ret;
}
