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

m::resource
account_deactivate
{
	"/_matrix/client/r0/account/deactivate",
	{
		"Deactivate the user's account, removing all ability for the user to login again. (3.3.3)"
	}
};

m::resource::response
post__deactivate(client &client,
                 const m::resource::request &request)
{
	const json::object &auth
	{
		request["auth"]
	};

	const json::string &type
	{
		auth.at("type")
	};

	const json::string &session
	{
		auth["session"]
	};

	m::user user
	{
		request.user_id
	};

	user.deactivate();

	return m::resource::response
	{
		client, json::members
		{
			{ "goodbye", "Thanks for giving us a try. Sorry it didn't work out." }
		}
	};
}

m::resource::method
post_deactivate
{
	account_deactivate, "POST", post__deactivate,
	{
		post_deactivate.REQUIRES_AUTH |
		post_deactivate.RATE_LIMITED
	}
};
