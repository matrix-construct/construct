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

m::resource
login_resource
{
	"/_matrix/client/r0/login",
	{
		"(3.3.1) Authenticates the user by password, and issues an access token "
		"they can use to authorize themself in subsequent requests."
	}
};

m::resource::response
post__login_password(client &client,
                     const m::resource::request::object<m::login> &request)
{
	const json::object &identifier
	{
		json::get<"identifier"_>(request)
	};

	const json::string &identifier_type
	{
		identifier.get("type")
	};

	if(identifier_type && identifier_type != "m.id.user")
		throw m::UNSUPPORTED
		{
			"Identifier type '%s' is not supported.", identifier_type
		};

	const json::string &username
	{
		identifier_type == "m.id.user"?
			json::string(identifier.at("user")):
			at<"user"_>(request)
	};

	const auto &localpart
	{
		valid(m::id::USER, username)?
			m::id::user(username).local():
			string_view{username}
	};

	const auto &hostpart
	{
		valid(m::id::USER, username)?
			m::id::user(username).host():
			my_host()
	};

	if(!my_host(hostpart))
		throw m::UNSUPPORTED
		{
			"Credentials for users of homeserver '%s' cannot be obtained here.",
			hostpart,
		};

	// Build a canonical MXID from a the user field
	const m::id::user::buf user_id
	{
		localpart, hostpart
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

	if(!active(user))
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
		valid(m::id::DEVICE, requested_device_id)?
			m::id::device::buf{requested_device_id}:
		requested_device_id?
			m::id::device::buf{requested_device_id, my_host()}:
			m::id::device::buf{m::id::generate, my_host()}
	};

	char access_token_buf[32];
	const string_view access_token
	{
		m::user::tokens::generate(access_token_buf)
	};

	char remote_buf[96];
	const json::value last_seen_ip
	{
		string(remote_buf, remote(client)), json::STRING
	};

	const m::room::id::buf tokens_room
	{
		"tokens", origin(m::my())
	};

	// Log the user in by issuing an event in the tokens room containing
	// the generated token. When this call completes without throwing the
	// access_token will be committed and the user will be logged in.
	const m::event::id::buf access_token_id
	{
		m::send(tokens_room, user_id, "ircd.access_token", access_token,
		{
			{ "ip",         last_seen_ip   },
			{ "device_id",  device_id      },
		})
	};

	const m::user::devices devices
	{
		user_id
	};

	devices.set(json::members
	{
		{ "device_id",        device_id                    },
		{ "display_name",     initial_device_display_name  },
		{ "last_seen_ts",     ircd::time<milliseconds>()   },
		{ "last_seen_ip",     last_seen_ip                 },
		{ "access_token_id",  access_token_id              },
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

m::resource::response
post__login(client &client,
            const m::resource::request::object<m::login> &request)
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

m::resource::method
method_post
{
	login_resource, "POST", post__login,
	{
		method_post.RATE_LIMITED
	}
};

m::resource::response
get__login(client &client,
           const m::resource::request &request)
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

m::resource::method
method_get
{
	login_resource, "GET", get__login,
	{
		method_get.RATE_LIMITED
	}
};
