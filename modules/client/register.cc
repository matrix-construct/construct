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

static m::resource::response
post__register_puppet(client &client, const m::resource::request::object<m::user::registar> &request);

static m::resource::response
post__register_guest(client &client, const m::resource::request::object<m::user::registar> &request);

static m::resource::response
post__register_user(client &client, const m::resource::request::object<m::user::registar> &request);

static m::resource::response
post__register(client &client, const m::resource::request::object<m::user::registar> &request);

m::resource
register_resource
{
	"/_matrix/client/r0/register",
	{
		"(3.3.1) Register for an account on this homeserver."
	}
};

m::resource::method
method_post
{
	register_resource, "POST", post__register,
	{
		method_post.RATE_LIMITED
	}
};

ircd::conf::item<bool>
register_enable
{
	{ "name",     "ircd.client.register.enable" },
	{ "default",  false                         }
};

/// see: ircd/m/register.h for the m::user::registar tuple.
///
m::resource::response
post__register(client &client,
               const m::resource::request::object<m::user::registar> &request)
{
	const json::object &auth
	{
		json::get<"auth"_>(request)
	};

	const auto &type
	{
		auth["type"]?
			json::string(auth["type"]):
			json::get<"type"_>(request)
	};

	// Branch for special spec-behavior when a client (or bridge) who is
	// already logged in hits this endpoint.
	if(request.bridge_id)
		return post__register_puppet(client, request);

	if(!type || type == "m.login.application_service")
		return m::resource::response
		{
			client, http::UNAUTHORIZED, json::object{flows}
		};

	if(!bool(register_enable))
		throw m::error
		{
			http::FORBIDDEN, "M_REGISTRATION_DISABLED",
			"Registration for this server is disabled."
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

m::resource::response
post__register_user(client &client,
                    const m::resource::request::object<m::user::registar> &request)
try
{
	if(!bool(register_user_enable))
		throw m::error
		{
			http::FORBIDDEN, "M_REGISTRATION_DISABLED",
			"User registration for this server is disabled."
		};

	const unique_buffer<mutable_buffer> buf
	{
		4_KiB
	};

	// upcast to the user::registar tuple
	const m::user::registar &registar
	{
		request
	};

	// call operator() to register the user and receive response output
	const json::object response
	{
		registar(buf, remote(client))
	};

	// Send response to user
	return m::resource::response
	{
		client, http::OK, response
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
	{ "default",  false                               }
};

m::resource::response
post__register_guest(client &client,
                     const m::resource::request::object<m::user::registar> &request)
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

	char access_token_buf[64];
	const string_view access_token
	{
		m::user::tokens::generate(access_token_buf)
	};

	return m::resource::response
	{
		client, http::OK,
		{
			{ "user_id",         user_id        },
			{ "home_server",     my_host()      },
			{ "access_token",    access_token   },
		}
	};
}

m::resource::response
post__register_puppet(client &client,
                      const m::resource::request::object<m::user::registar> &request)
try
{
	assert(request.bridge_id);
	m::user::registar registar
	{
		request
	};

	// Help out non-spec-compliant bridges
	if(!json::get<"type"_>(registar))
		json::get<"type"_>(registar) = "m.login.application_service"_sv;

	const auto kind
	{
		request.query["kind"]
	};

	// Sanity condition to reject this kind; note we don't require any other
	// specific string here like "user" or "bridge" for forward spec-compat.
	if(kind == "guest")
		throw m::UNSUPPORTED
		{
			"Obtaining a guest access token when you're already registered"
			" and logged in is not yet supported."
		};

	const unique_buffer<mutable_buffer> buf
	{
		4_KiB
	};

	// call operator() to register the user and receive response output
	const json::object response
	{
		registar(buf, remote(client))
	};

	// Send response to user
	return m::resource::response
	{
		client, http::OK, response
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

const std::string
flows{R"({
	"flows":
	[
		{
			"stages":
			[
				"m.login.dummy"
			]
		},
		{
			"stages":
			[
				"m.login.dummy",
				"m.login.email.identity"
			]
		}
	]
})"};
