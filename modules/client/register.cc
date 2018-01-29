/*
 * Copyright (C) 2017 Charybdis Development Team
 * Copyright (C) 2017 Jason Volk <jason@zemos.net>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice is present in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

using namespace ircd;

namespace { namespace name
{
	constexpr const auto username {"username"};
	constexpr const auto bind_email {"bind_email"};
	constexpr const auto password {"password"};
	constexpr const auto auth {"auth"};
}}

struct body
:json::tuple
<
	json::property<name::username, string_view>,
	json::property<name::bind_email, bool>,
	json::property<name::password, string_view>,
	json::property<name::auth, json::object>
>
{
	using super_type::tuple;
};

static void validate_user_id(const m::id::user &user_id);
static void validate_password(const string_view &password);

resource::response
handle_post_kind_user(client &client,
                      const resource::request::object<body> &request)
try
{
	// 3.3.1 Additional authentication information for the user-interactive authentication API.
	const auto &auth
	{
		at<"auth"_>(request)
	};

	// 3.3.1 Required. The login type that the client is attempting to complete.
	const auto &type
	{
		unquote(auth.at("type"))
	};

	// We only support this for now, for some reason. TODO: XXX
	if(type != "m.login.dummy")
		throw m::error
		{
			"M_UNSUPPORTED", "Registration '%s' not supported.", type
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

	// Activate the account. Underneath this will join the user to the room
	// !accounts:your.domain. If the user_id is already a member then this
	// throws 409 Conflict; otherwise the user is registered after this call.
	user.activate(
	{
		{ "bind_email", bind_email },
	});

	// Set the password for the account. This issues an ircd.password state
	// event to the accounts room for the user. If this call completes the
	// user will be able to login with m.login.password
	user.password(password);

	// Send response to user
	return resource::response
	{
		client, http::CREATED,
		{
			{ "user_id",         user_id        },
			{ "home_server",     my_host()      },
//			{ "access_token",    access_token   },
		}
	};
}
catch(const m::INVALID_MXID &e)
{
	throw m::error
	{
		http::BAD_REQUEST,
		"M_INVALID_USERNAME",
		"Not a valid username. Please try again."
	};
};

resource::response
handle_post_kind_guest(client &client,
                       const resource::request::object<body> &request)
{
	const m::id::user::buf user_id
	{
		m::generate, my_host()
	};

	return resource::response
	{
		client, http::CREATED,
		{
			{ "user_id",         user_id        },
			{ "home_server",     my_host()      },
//			{ "access_token",    access_token   },
		}
	};
}

resource::response
handle_post(client &client,
            const resource::request::object<body> &request)
{
	const auto kind
	{
		request.query["kind"]
	};

	if(kind == "guest")
		return handle_post_kind_guest(client, request);

	if(kind.empty() || kind == "user")
		return handle_post_kind_user(client, request);

	throw m::error
	{
		http::BAD_REQUEST,
		"M_UNKNOWN",
		"Unknown 'kind' of registration specified in query."
	};
}

resource register_resource
{
	"/_matrix/client/r0/register",
	{
		"Register for an account on this homeserver. (3.3.1)"
	}
};

resource::method post
{
	register_resource, "POST", handle_post
};

mapi::header IRCD_MODULE
{
	"registers the resource 'client/register' to handle requests"
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
