// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include "user.h"

using namespace ircd;

static m::resource::response
post__openid__request_token(client &client,
                            const m::resource::request &request,
                            const m::user::id &user_id);

m::resource::response
post__openid(client &client,
             const m::resource::request &request,
             const m::user::id &user_id)
{
	if(user_id != request.user_id)
		throw m::FORBIDDEN
		{
			"Trying to post openid for `%s' but you are `%s'",
			user_id,
			request.user_id
		};

	// request.parv[0] = <user_id>
	// request.parv[1] = "openid"
	const string_view &cmd
	{
		request.parv[2]
	};

	if(cmd == "request_token")
		return post__openid__request_token(client, request, user_id);

	throw m::NOT_FOUND
	{
		"/user/openid command not found"
	};
}

m::resource::response
post__openid__request_token(client &client,
                            const m::resource::request &request,
                            const m::user::id &user_id)
{
	const string_view &matrix_server_name
	{
		my_host()
	};

	const string_view &token_type
	{
		"Bearer"
	};

	const long &expires_in
	{
		3600L
	};

	const auto access_token
	{
		request.access_token
	};

	assert(access_token);
	return m::resource::response
	{
		client, http::OK, json::members
		{
			{ "matrix_server_name",  matrix_server_name  },
			{ "token_type",          token_type          },
			{ "expires_in",          expires_in          },
			{ "access_token",        access_token        },
		}
	};
}
