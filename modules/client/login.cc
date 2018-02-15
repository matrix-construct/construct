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

namespace { namespace name
{
	constexpr const auto password{"password"};
	constexpr const auto medium{"medium"};
	constexpr const auto type{"type"};
	constexpr const auto user{"user"};
	constexpr const auto address{"address"};
}}

struct body
:json::tuple
<
	json::property<name::password, string_view>,
	json::property<name::medium, time_t>,
	json::property<name::type, string_view>,
	json::property<name::user, string_view>,
	json::property<name::address, string_view>
>
{
	using super_type::tuple;
};

resource::response
post_login_password(client &client,
                    const resource::request::object<body> &request)
{
	// Build a canonical MXID from a the user field
	const m::id::user::buf user_id
	{
		unquote(at<"user"_>(request)), my_host()
	};

	const auto &supplied_password
	{
		unquote(at<"password"_>(request))
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

	// Generate the access token
	static constexpr const auto token_len{127};
	static const auto token_dict{rand::dict::alpha};
	char token_buf[token_len + 1];
	const string_view access_token
	{
		rand::string(token_dict, token_len, token_buf, sizeof(token_buf))
	};

	// Log the user in by issuing an event in the tokens room containing
	// the generated token. When this call completes without throwing the
	// access_token will be committed and the user will be logged in.
	m::send(m::user::tokens, user_id, "ircd.access_token", access_token,
	{
		{ "ip",      string(remote(client)) },
		{ "device",  "unknown"              },
	});

	// Send response to user
	return resource::response
	{
		client,
		{
			{ "user_id",        user_id        },
			{ "home_server",    my_host()      },
			{ "access_token",   access_token   },
		}
	};
}

resource::response
post_login(client &client, const resource::request::object<body> &request)
{
	// x.x.x Required. The login type being used.
	// Currently only "m.login.password" is supported.
	const auto type
	{
		unquote(at<"type"_>(request))
	};

	if(type == "m.login.password")
		return post_login_password(client, request);
	else
		throw m::error
		{
			"M_UNSUPPORTED", "Login type is not supported."
		};
}

resource::method method_post
{
	login_resource, "POST", post_login
};

resource::response
get_login(client &client, const resource::request &request)
{
	json::member login_password
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
	login_resource, "GET", get_login
};
