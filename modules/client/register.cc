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

namespace { namespace name
{
	constexpr const auto username {"username"};
	constexpr const auto bind_email {"bind_email"};
	constexpr const auto password {"password"};
	constexpr const auto auth {"auth"};
	constexpr const auto device_id {"device_id"};
}}

struct body
:json::tuple
<
	json::property<name::username, string_view>,
	json::property<name::bind_email, bool>,
	json::property<name::password, string_view>,
	json::property<name::auth, json::object>,
	json::property<name::device_id, string_view>
>
{
	using super_type::tuple;
};

extern "C" m::event::id::buf register__user(const m::user &user, const json::members &contents);
static void validate_user_id(const m::id::user &user_id);
static void validate_password(const string_view &password);

resource::response
post__register_user(client &client,
                    const resource::request::object<body> &request)
try
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
		unquote(json::get<"username"_>(request))
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
		unquote(at<"password"_>(request))
	};

	// (r0.3.0) 3.4.1 ID of the client device. If this does not correspond to a
	// known client device, a new device will be created. The server will auto-
	// generate a device_id if this is not specified.
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

	// 3.3.1 If true, the server binds the email used for authentication to the
	// Matrix ID with the ID Server. Defaults to false.
	const auto &bind_email
	{
		get<"bind_email"_>(request, false)
	};

	// Check if the password is acceptable for this server or throws
	validate_password(password);

	// Represent the user
	m::user user
	{
		user_id
	};

	// Activate the account. Underneath this will create a special room
	// for this user in the form of !@user:host and set a key in !users:host
	// If the user_id is taken this throws 409 Conflict because those assets
	// will already exist; otherwise the user is registered after this call.
	user.activate(
	{
		{ "bind_email", bind_email },
	});

	// Set the password for the account. This issues an ircd.password state
	// event to the user's room. User will be able to login with
	// m.login.password
	user.password(password);

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
		{ "ip",      string(remote(client))  },
		{ "device",  device_id               },
	});

	// Send response to user
	return resource::response
	{
		client, http::CREATED,
		{
			{ "user_id",         user_id        },
			{ "home_server",     my_host()      },
			{ "access_token",    access_token   },
			{ "device_id",       device_id      },
		}
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

resource::response
post__register_guest(client &client,
                     const resource::request::object<body> &request)
{
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

resource::response
post__register(client &client,
               const resource::request::object<body> &request)
{
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

/// Register the user by creating a room !@user:myhost and then setting a
/// an `ircd.account` state event in the `users` room.
///
/// Each of the registration options becomes a key'ed state event in the
/// user's room.
///
/// Once this call completes the registration was successful; otherwise
/// throws.
m::event::id::buf
register__user(const m::user &user,
               const json::members &contents)
try
{
	const m::room::id user_room_id
	{
		user.room_id()
	};

	m::room user_room
	{
		create(user_room_id, m::me.user_id, "user")
	};

	send(user_room, user.user_id, "ircd.account.options", "registration", contents);
	return send(user.users, m::me.user_id, "ircd.user", user.user_id,
	{
		{ "active", true }
	});
}
catch(const m::ALREADY_MEMBER &e)
{
	throw m::error
	{
		http::CONFLICT, "M_USER_IN_USE", "The desired user ID is already in use."
	};
}

static void _first_user_registered(const m::event &event);

const m::hook
_first_user_hook
{
	_first_user_registered,
	{
		{ "_site",    "vm notify"         },
		{ "room_id",  "!users:zemos.net"  },
		{ "type",     "ircd.user"         },
	},
};

void
_first_user_registered(const m::event &event)
{
	static bool already;
	if(already)
		return;

	const m::room::state state
	{
		m::room{at<"room_id"_>(event)}
	};

	const auto &count
	{
		state.count("ircd.user")
	};

	//TODO: This will rot as soon as more auto-users are created
	//TODO: along with @ircd. Need a way to know when the first HUMAN
	//TODO: has registered.
	if(count != 2) // @ircd + first user
	{
		if(count > 2)
			already = true;

		return;
	}

	const m::user::id user
	{
		at<"state_key"_>(event)
	};

	join(m::control, user);
	already = true;
}
