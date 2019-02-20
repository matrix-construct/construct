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
	"Client 3.4.1 :Register"
};

extern const std::string
flows;

static void
validate_password(const string_view &password);

extern "C" void
validate_user_id(const m::id::user &user_id);

extern "C" std::string
register_user(const m::registar &,
              const client *const & = nullptr,
              const bool &gen_token = false);

static resource::response
post__register_guest(client &client, const resource::request::object<m::registar> &request);

static resource::response
post__register_user(client &client, const resource::request::object<m::registar> &request);

static resource::response
post__register(client &client, const resource::request::object<m::registar> &request);

resource
register_resource
{
	"/_matrix/client/r0/register",
	{
		"(3.3.1) Register for an account on this homeserver."
	}
};

resource::method
method_post
{
	register_resource, "POST", post__register
};

ircd::conf::item<bool>
register_enable
{
	{ "name",     "ircd.client.register.enable" },
	{ "default",  true                          }
};

/// see: ircd/m/register.h for the m::registar tuple.
///
resource::response
post__register(client &client,
               const resource::request::object<m::registar> &request)
{
	if(!bool(register_enable))
		throw m::error
		{
			http::UNAUTHORIZED, "M_REGISTRATION_DISABLED",
			"Registration for this server is disabled."
		};

	const json::object &auth
	{
		json::get<"auth"_>(request)
	};

	if(empty(auth))
		return resource::response
		{
			client, http::UNAUTHORIZED, json::object{flows}
		};

	const auto kind
	{
		request.query["kind"]
	};

	if(kind == "guest")
		return post__register_guest(client, request);

	if(kind.empty() || kind == "user")
		return post__register_user(client, request);

	throw m::UNSUPPORTED
	{
		"Unknown 'kind' of registration specified in query."
	};
}

ircd::conf::item<bool>
register_user_enable
{
	{ "name",     "ircd.client.register.user.enable" },
	{ "default",  true                               }
};

resource::response
post__register_user(client &client,
                    const resource::request::object<m::registar> &request)
try
{
	if(!bool(register_user_enable))
		throw m::error
		{
			http::UNAUTHORIZED, "M_REGISTRATION_DISABLED",
			"User registration for this server is disabled."
		};

	const bool &inhibit_login
	{
		json::get<"inhibit_login"_>(request)
	};

	const std::string response
	{
		register_user(request, &client, !inhibit_login)
	};

	// Send response to user
	return resource::response
	{
		client, http::CREATED, json::object{response}
	};
}
catch(const m::INVALID_MXID &e)
{
	throw m::error
	{
		http::BAD_REQUEST, "M_INVALID_USERNAME",
		"Not a valid username. Please try again."
	};
};

ircd::conf::item<bool>
register_guest_enable
{
	{ "name",     "ircd.client.register.guest.enable" },
	{ "default",  true                                }
};

resource::response
post__register_guest(client &client,
                     const resource::request::object<m::registar> &request)
{
	if(!bool(register_guest_enable))
		throw m::error
		{
			http::FORBIDDEN, "M_GUEST_DISABLED",
			"Guest access is disabled"
		};

	const m::id::user::buf user_id
	{
		m::generate, my_host()
	};

	char access_token_buf[32];
	const string_view access_token
	{
		m::user::gen_access_token(access_token_buf)
	};

	return resource::response
	{
		client, http::CREATED,
		{
			{ "user_id",         user_id        },
			{ "home_server",     my_host()      },
			{ "access_token",    access_token   },
		}
	};
}

std::string
register_user(const m::registar &request,
              const client *const &client,
              const bool &gen_token)
{
	// 3.3.1 Additional authentication information for the user-interactive authentication API.
	const json::object &auth
	{
		json::get<"auth"_>(request)
	};

	// 3.3.1 The login type that the client is attempting to complete.
	const string_view &type
	{
		!empty(auth)? unquote(auth.at("type")) : string_view{}
	};

	// We only support this for now, for some reason. TODO: XXX
	if(type && type != "m.login.dummy")
		throw m::UNSUPPORTED
		{
			"Registration '%s' not supported.", type
		};

	// 3.3.1 The local part of the desired Matrix ID. If omitted, the homeserver MUST
	// generate a Matrix ID local part.
	const auto &username
	{
		json::get<"username"_>(request)
	};

	// Generate canonical mxid. The home_server is appended if one is not
	// specified. We do not generate a user_id here if the local part is not
	// specified. TODO: isn't that guest reg?
	const m::id::user::buf user_id
	{
		username, my_host()
	};

	// Check if the the user_id is acceptably formed for this server or throws
	validate_user_id(user_id);

	// 3.3.1 Required. The desired password for the account.
	const auto &password
	{
		at<"password"_>(request)
	};

	// (r0.3.0) 3.4.1 ID of the client device. If this does not correspond to a
	// known client device, a new device will be created. The server will auto-
	// generate a device_id if this is not specified.
	const auto requested_device_id
	{
		json::get<"device_id"_>(request)
	};

	const m::id::device::buf device_id
	{
		requested_device_id?
			m::id::device::buf{requested_device_id, my_host()}:
		gen_token?
			m::id::device::buf{m::id::generate, my_host()}:
			m::id::device::buf{}
	};

	const auto &initial_device_display_name
	{
		json::get<"initial_device_display_name"_>(request)
	};

	// 3.3.1 If true, the server binds the email used for authentication to the
	// Matrix ID with the ID Server. Defaults to false.
	const auto &bind_email
	{
		get<"bind_email"_>(request, false)
	};

	// Check if the password is acceptable for this server or throws
	validate_password(password);

	//TODO: ABA
	if(exists(user_id))
		throw m::error
		{
			http::CONFLICT, "M_USER_IN_USE",
			"The desired user ID is already in use."
		};

	//TODO: ABA / TXN
	// Represent the user
	m::user user
	{
		m::create(user_id)
	};

	// Activate the account. Underneath this will create a special room
	// for this user in the form of !@user:host and set a key in !users:host
	// If the user_id is taken this throws 409 Conflict because those assets
	// will already exist; otherwise the user is registered after this call.
	//TODO: ABA / TXN
	user.activate();

	// Set the password for the account. This issues an ircd.password state
	// event to the user's room. User will be able to login with
	// m.login.password
	user.password(password);

	// Store the options from registration.
	m::user::room user_room{user};
	send(user_room, user.user_id, "ircd.account.options", "registration",
	{
		{ "bind_email", bind_email },
	});

	char access_token_buf[32];
	const string_view access_token
	{
		gen_token?
			m::user::gen_access_token(access_token_buf):
			string_view{}
	};

	// Log the user in by issuing an event in the tokens room containing
	// the generated token. When this call completes without throwing the
	// access_token will be committed and the user will be logged in.
	if(gen_token)
		m::send(m::user::tokens, user_id, "ircd.access_token", access_token,
		{
			{ "ip",      client? string(remote(*client)) : std::string{} },
			{ "device",  device_id                                       },
		});

	if(gen_token)
		m::device::set(user_id,
		{
			{ "device_id",     device_id                    },
			{ "display_name",  initial_device_display_name  },
			{ "last_seen_ts",  ircd::time<milliseconds>()   },
			{ "last_seen_ip",  string(remote(*client))      },
		});

	// Send response to user
	return json::strung
	{
		json::members
		{
			{ "user_id",         user_id        },
			{ "home_server",     my_host()      },
			{ "access_token",    access_token   },
			{ "device_id",       device_id      },
		}
	};
}

void
validate_user_id(const m::id::user &user_id)
{
	if(user_id.host() != my_host())
		throw m::error
		{
			http::BAD_REQUEST,
			"M_INVALID_USERNAME",
			"Can only register with host '%s'",
			my_host()
		};
}

void
validate_password(const string_view &password)
{
	if(password.size() > 255)
		throw m::error
		{
			http::BAD_REQUEST,
			"M_INVALID_PASSWORD",
			"The desired password is too long"
		};
}

const std::string
flows
{R"({
	"flows":
	[
		{
			"stages":
			[
				"m.login.dummy", "m.login.password"
			]
		}
	]
})"};
