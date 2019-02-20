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

mapi::header
IRCD_MODULE
{
	"Client 3.3 :Login"
};

resource
login_resource
{
	"/_matrix/client/r0/login",
	{
		"(3.3.1) Authenticates the user by password, and issues an access token "
		"they can use to authorize themself in subsequent requests."
	}
};

resource::response
post__login_password(client &client,
                     const resource::request::object<m::login> &request)
{
	// Build a canonical MXID from a the user field
	const m::id::user::buf user_id
	{
		at<"user"_>(request), my_host()
	};

	const string_view &supplied_password
	{
		at<"password"_>(request)
	};

	m::user user
	{
		user_id
	};

	if(!user.is_password(supplied_password))
		throw m::FORBIDDEN
		{
			"Access denied."
		};

	if(!user.is_active())
		throw m::FORBIDDEN
		{
			"Access denied."
		};

	const string_view &requested_device_id
	{
		json::get<"device_id"_>(request)
	};

	const string_view &initial_device_display_name
	{
		json::get<"initial_device_display_name"_>(request)
	};

	const auto device_id
	{
		requested_device_id?
			m::id::device::buf{requested_device_id, my_host()}:
			m::id::device::buf{m::id::generate, my_host()}
	};

	char access_token_buf[32];
	const string_view access_token
	{
		m::user::gen_access_token(access_token_buf)
	};

	// Log the user in by issuing an event in the tokens room containing
	// the generated token. When this call completes without throwing the
	// access_token will be committed and the user will be logged in.
	m::send(m::user::tokens, user_id, "ircd.access_token", access_token,
	{
		{ "ip",         string(remote(client)) },
		{ "device_id",  device_id              },
	});

	m::device::set(user_id,
	{
		{ "device_id",     device_id                    },
		{ "display_name",  initial_device_display_name  },
		{ "last_seen_ts",  ircd::time<milliseconds>()   },
		{ "last_seen_ip",  string(remote(client))       },
	});

	// Send response to user
	return resource::response
	{
		client, json::members
		{
			{ "user_id",        user_id        },
			{ "home_server",    my_host()      },
			{ "access_token",   access_token   },
			{ "device_id",      device_id      },
		}
	};
}

resource::response
post__login(client &client,
            const resource::request::object<m::login> &request)
{
	const auto &type
	{
		at<"type"_>(request)
	};

	if(type == "m.login.password")
		return post__login_password(client, request);

	throw m::UNSUPPORTED
	{
		"Login type '%s' is not supported.", type
	};
}

resource::method
method_post
{
	login_resource, "POST", post__login
};

resource::response
get__login(client &client,
           const resource::request &request)
{
	const json::member login_password
	{
		"type", "m.login.password"
	};

	json::value flows[1]
	{
		{ login_password }
	};

	return resource::response
	{
		client, json::members
		{
			{ "flows", { flows, 1 } }
		}
	};
}

resource::method
method_get
{
	login_resource, "GET", get__login
};
