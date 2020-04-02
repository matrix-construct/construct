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

static m::resource::response
post__logout(client &,
             const m::resource::request &);

static m::resource::response
post__logout_all(client &,
                 const m::resource::request &);

mapi::header
IRCD_MODULE
{
	"Client 3.3.2 :Logout"
};

m::resource
logout_resource
{
	"/_matrix/client/r0/logout",
	{
		"(3.3.2) Invalidates an existing access token, so that it can no"
		" longer be used for authorization."
	}
};

m::resource
logout_all_resource
{
	"/_matrix/client/r0/logout/all",
	{
		"(5.4.4) Invalidates all access tokens for a user, so that they can no"
		" longer be used for authorization. This includes the access token"
		" that made this request."
	}
};

m::resource::method
post_method
{
	logout_resource, "POST", post__logout,
	{
		post_method.REQUIRES_AUTH
	}
};

m::resource::method
post_all_method
{
	logout_all_resource, "POST", post__logout_all,
	{
		post_all_method.REQUIRES_AUTH
	}
};

m::resource::response
post__logout(client &client,
             const m::resource::request &request)
{
	const auto &access_token
	{
		request.access_token
	};

	const m::user::tokens tokens
	{
		request.user_id
	};

	const bool deleted
	{
		tokens.del(access_token, "client logout")
	};

	return m::resource::response
	{
		client, http::OK
	};
}

m::resource::response
post__logout_all(client &client,
                 const m::resource::request &request)
{
	const m::user::tokens tokens
	{
		request.user_id
	};

	const size_t invalidations
	{
		tokens.del("client logout all")
	};

	return m::resource::response
	{
		client, json::members
		{
			{ "invalidations", long(invalidations) }
		}
	};
}
