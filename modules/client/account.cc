// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

using namespace ircd;

struct account
:ircd::resource
{
	resource deactivate
	{
		"/_matrix/client/r0/account/deactivate",
		{
			"Deactivate the user's account, removing all ability for the user to login again. (3.3.3)"
		}
	};

	resource password
	{
		"/_matrix/client/r0/account/password",
		{
			"Changes the password for an account on this homeserver. (3.3.4)"
		}
	};

	using resource::resource;
}
account_resource
{
	"/_matrix/client/r0/account",
	{
		"Account management (3.3)"
	}
};

resource::response
account_password(client &client, const resource::request &request)
try
{
	const string_view &new_password
	{
		unquote(request.at("new_password"))
	};

	const string_view &type
	{
		unquote(request.at({"auth", "type"}))
	};

	if(type != "m.login.password")
		throw m::error
		{
			"M_UNSUPPORTED", "Login type is not supported."
		};

	const string_view &session
	{
		request[{"auth", "session"}]
	};

	m::user user
	{
		request.user_id
	};

	user.password(new_password);
	return resource::response
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

resource::method post_password
{
	account_resource.password, "POST", account_password,
	{
		post_password.REQUIRES_AUTH
	}
};

resource::response
account_deactivate(client &client, const resource::request &request)
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
			{ "goodbye", "Thanks for visiting. Come back soon!" }
		}
	};
}

resource::method post_deactivate
{
	account_resource.deactivate, "POST", account_deactivate,
	{
		post_deactivate.REQUIRES_AUTH
	}
};

mapi::header IRCD_MODULE
{
	"registers the resource 'client/account' to handle requests"
};
