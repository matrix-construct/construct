// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

ircd::json::object
ircd::m::user::registar::operator()(const mutable_buffer &out,
                                    const net::ipport &remote)
const
{
	// 3.3.1 Additional authentication information for the user-interactive authentication API.
	const json::object &auth
	{
		json::get<"auth"_>(*this)
	};

	// 3.3.1 The login type that the client is attempting to complete.
	const json::string &type
	{
		auth["type"]?
			json::string(auth["type"]):
			json::get<"type"_>(*this)
	};

	// We only support this for now, for some reason. TODO: XXX
	if(type && (type != "m.login.dummy" && type != "m.login.application_service"))
		throw m::UNSUPPORTED
		{
			"Registration '%s' not supported.", type
		};

	// 3.3.1 The local part of the desired Matrix ID. If omitted, the homeserver MUST
	// generate a Matrix ID local part.
	const string_view username
	{
		valid(m::id::USER, json::get<"username"_>(*this))?
			m::id::user(json::get<"username"_>(*this)).localname():
			string_view(json::get<"username"_>(*this))
	};

	// Generate canonical mxid. The home_server is appended if one is not
	// specified. We do not generate a user_id here if the local part is not
	// specified. TODO: isn't that guest reg?
	const m::id::user::buf user_id
	{
		username, origin(my())
	};

	// Check if the the user_id is acceptably formed for this server or throws
	validate_user_id(user_id);

	// 3.3.1 Required. The desired password for the account.
	const auto &password
	{
		json::get<"password"_>(*this)
	};

	// (r0.3.0) 3.4.1 ID of the client device. If this does not correspond to a
	// known client device, a new device will be created. The server will auto-
	// generate a device_id if this is not specified.
	const auto requested_device_id
	{
		json::get<"device_id"_>(*this)
	};

	const m::id::device::buf device_id
	{
		requested_device_id?
			m::id::device::buf{requested_device_id, my_host()}:
		!json::get<"inhibit_login"_>(*this)?
			m::id::device::buf{m::id::generate, my_host()}:
			m::id::device::buf{}
	};

	const auto &initial_device_display_name
	{
		json::get<"initial_device_display_name"_>(*this)
	};

	// 3.3.1 If true, the server binds the email used for authentication to the
	// Matrix ID with the ID Server. Defaults to false.
	const auto &bind_email
	{
		json::get<"bind_email"_>(*this, false)
	};

	// Check if the password is acceptable for this server or throws
	if(type != "m.login.application_service")
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
	if(type != "m.login.application_service")
		user.password(password);

	// Represent the user's room; was created in m::create(user_id)
	m::user::room user_room
	{
		user
	};

	// Store the options from registration.
	if(type != "m.login.application_service")
		send(user_room, user.user_id, "ircd.account.options", "registration", json::members
		{
			{ "bind_email", bind_email },
		});

	// Optionally generate an access_token for login.
	char access_token_buf[32];
	const string_view access_token
	{
		!json::get<"inhibit_login"_>(*this)?
			m::user::tokens::generate(access_token_buf):
			string_view{}
	};

	// Log the user in by issuing an event in the tokens room containing
	// the generated token. When this call completes without throwing the
	// access_token will be committed and the user will be logged in.
	if(type != "m.login.application_service" && !json::get<"inhibit_login"_>(*this))
	{
		char remote_buf[96];
		const json::value last_seen_ip
		{
			remote?
				string(remote_buf, remote):
				string_view{},

			json::STRING
		};

		const m::room::id::buf user_tokens
		{
			"tokens", user_id.host()
		};

		const m::event::id::buf access_token_id
		{
			m::send(user_tokens, user_id, "ircd.access_token", access_token, json::members
			{
				{ "ip",         last_seen_ip },
				{ "device_id",  device_id    },
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
	}

	// Send response to user
	return json::stringify(mutable_buffer{out}, json::members
	{
		{ "user_id",         user_id        },
		{ "home_server",     my_host()      },
		{ "access_token",    access_token   },
		{ "device_id",       device_id      },
	});
}

void
ircd::m::user::registar::validate_password(const string_view &password)
{
	static const size_t &max
	{
		255
	};

	if(password.size() > max)
		throw m::error
		{
			http::BAD_REQUEST, "M_INVALID_PASSWORD",
			"The desired password exceeds %zu characters",
			max,
		};

	if(!password)
		throw m::error
		{
			http::BAD_REQUEST, "M_INVALID_PASSWORD",
			"Required password was not submitted.",
		};
}

void
ircd::m::user::registar::validate_user_id(const m::user::id &user_id)
{
	if(user_id.host() != my_host())
		throw m::error
		{
			http::BAD_REQUEST, "M_INVALID_USERNAME",
			"Can only register with host '%s'",
			my_host()
		};
}
