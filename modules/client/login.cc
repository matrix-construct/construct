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
	constexpr const auto type{"type"};
	constexpr const auto user{"user"};
	constexpr const auto medium{"medium"};
	constexpr const auto address{"address"};
	constexpr const auto password{"password"};
	constexpr const auto token{"token"};
	constexpr const auto device_id{"device_id"};
	constexpr const auto initial_device_display_name{"initial_device_display_name"};
}}

struct body
:json::tuple
<
	/// Required. The login type being used. One of: ["m.login.password",
	/// "m.login.token"]
	json::property<name::type, string_view>,

	/// The fully qualified user ID or just local part of the user ID, to
	/// log in.
	json::property<name::user, string_view>,

	/// When logging in using a third party identifier, the medium of the
	/// identifier. Must be 'email'.
	json::property<name::medium, string_view>,

	/// Third party identifier for the user.
	json::property<name::address, string_view>,

	/// Required when type is m.login.password. The user's password.
	json::property<name::password, string_view>,

	/// Required when type is m.login.token. The login token.
	json::property<name::token, string_view>,

	/// ID of the client device. If this does not correspond to a known client
	/// device, a new device will be created. The server will auto-generate a
	/// device_id if this is not specified.
	json::property<name::device_id, string_view>,

	/// A display name to assign to the newly-created device. Ignored if
	/// device_id corresponds to a known device.
	json::property<name::initial_device_display_name, string_view>
>
{
	using super_type::tuple;
};

resource::response
post__login_password(client &client,
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

	const auto requested_device_id
	{
		unquote(json::get<"device_id"_>(request))
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
		{ "ip",      string(remote(client)) },
		{ "device",  device_id              },
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
            const resource::request::object<body> &request)
{
	const auto &type
	{
		unquote(at<"type"_>(request))
	};

	if(type == "m.login.password")
		return post__login_password(client, request);

	throw m::UNSUPPORTED
	{
		"Login type is not supported."
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
