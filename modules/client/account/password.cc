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
account_password
{
	"/_matrix/client/r0/account/password",
	{
		"Changes the password for an account on this homeserver. (3.3.4)"
	}
};

m::resource::response
post__password(client &client,
               const m::resource::request &request)
try
{
	const json::string &new_password
	{
		request.at("new_password")
	};

	const json::object &auth
	{
		request["auth"]
	};

	const json::string &type
	{
		auth.at("type")
	};

	if(type != "m.login.password")
		throw m::error
		{
			"M_UNSUPPORTED", "Login type is not supported."
		};

	const json::string &session
	{
		auth["session"]
	};

	m::user user
	{
		request.user_id
	};

	user.password(new_password);
	return m::resource::response
	{
		client, http::OK
	};
}
catch(const db::not_found &e)
{
	throw m::error
	{
		http::FORBIDDEN, "M_FORBIDDEN", "Access denied."
	};
}

m::resource::method
post_password
{
	account_password, "POST", post__password,
	{
		post_password.REQUIRES_AUTH
	}
};
