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

resource
account_deactivate
{
	"/_matrix/client/r0/account/deactivate",
	{
		"Deactivate the user's account, removing all ability for the user to login again. (3.3.3)"
	}
};

resource::response
post__deactivate(client &client,
                 const resource::request &request)
{
	const string_view &type
	{
		request.at({"auth", "type"})
	};

	const string_view &session
	{
		request[{"auth", "session"}]
	};

	m::user user
	{
		request.user_id
	};

	user.deactivate();

	return resource::response
	{
		client, json::members
		{
			{ "goodbye", "Thanks for giving us a try. Sorry it didn't work out." }
		}
	};
}

resource::method
post_deactivate
{
	account_deactivate, "POST", post__deactivate,
	{
		post_deactivate.REQUIRES_AUTH
	}
};

extern "C" m::event::id::buf
deactivate__user(const m::user &user,
                 const json::members &)
{
	const m::user::room user_room
	{
		user
	};

	return send(user_room, m::me.user_id, "ircd.account", "active", json::members
	{
		{ "value", false }
	});
}
