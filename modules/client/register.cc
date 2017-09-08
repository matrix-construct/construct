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

const auto home_server
{
	// "The hostname of the homeserver on which the account has been registered."
	"cdc.z"
};

namespace name
{
    constexpr auto username{"username"};
    constexpr auto bind_email{"bind_email"};
    constexpr auto password{"password"};
    constexpr auto auth{"auth"};
}

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

// Generate !accounts:host which is the MXID of the room where the members
// are the account registrations on this homeserver.
const m::id::room::buf accounts_room_id
{
	"accounts", home_server
};

m::room accounts_room
{
	accounts_room_id
};

static void
join_accounts_room(const m::id::user &user_id,
                   const json::members &contents)
try
{
	const json::builder content
	{
		nullptr, &contents
	};

	accounts_room.join(user_id, content);
}
catch(const m::ALREADY_MEMBER &e)
{
	throw m::error
	{
		http::CONFLICT, "M_USER_IN_USE", "The desired user ID is already in use."
	};
}

resource::response
handle_post_kind_user(client &client,
                      resource::request &request,
                      const resource::request::body<body> &body)
{
	// 3.3.1 Additional authentication information for the user-interactive authentication API.
	const auto &auth
	{
		at<name::auth>(body)
	};

	// 3.3.1 Required. The login type that the client is attempting to complete.
	const auto &type
	{
		unquote(auth.at("type"))
	};

	if(type != "m.login.dummy")
		throw m::error
		{
			"M_UNSUPPORTED", "Registration '%s' not supported.", type
		};

	// 3.3.1 The local part of the desired Matrix ID. If omitted, the homeserver MUST
	// generate a Matrix ID local part.
	const auto &username
	{
		unquote(get<name::username>(body))
	};

	// Generate canonical mxid
	const m::id::user::buf user_id
	{
		username, home_server
	};

	if(!user_id.valid())
		throw m::error
		{
			"M_INVALID_USERNAME", "The desired user ID is not a valid user name."
		};

	if(user_id.host() != home_server)
		throw m::error
		{
			"M_INVALID_USERNAME", "Can only register with host '%s'", home_server
		};

	// 3.3.1 Required. The desired password for the account.
	const auto &password
	{
		at<name::password>(body)
	};

	if(password.size() > 255)
		throw m::error
		{
			"M_INVALID_PASSWORD", "The desired password is too long"
		};

	// 3.3.1 If true, the server binds the email used for authentication to the
	// Matrix ID with the ID Server. Defaults to false.
	const auto &bind_email
	{
		get<name::bind_email>(body, false)
	};

	// Register the user by joining them to the accounts room. Join the user
	// to the accounts room by issuing a join event.  The content will store
	// keys from the registration options including the password - do not
	// expose this to clients //TODO: store hashed pass
	// Once this call completes the join was successful and the user
	// is registered, otherwise throws.
	char content[384];
	join_accounts_room(user_id,
	{
		{ "password",       password   },
		{ "bind_email",     bind_email },
	});

	// Send response to user
	return resource::response
	{
		client, http::CREATED, json::index
		{
			{ "user_id",         user_id        },
			{ "home_server",     home_server    },
//			{ "access_token",    access_token   },
		}
	};
}

resource::response
handle_post_kind_guest(client &client,
                       resource::request &request,
                       const resource::request::body<body> &body)
{
	const m::id::user::buf user_id
	{
		m::generate, home_server
	};

	return resource::response
	{
		client, http::CREATED, json::index
		{
			{ "user_id",         user_id        },
			{ "home_server",     home_server    },
			//{ "access_token",    access_token   },
		}
	};
}

resource::response
handle_post(client &client,
            resource::request &request)
{
	const resource::request::body<body> body
	{
		request
	};

	const auto kind
	{
		request.query["kind"]
	};

	if(kind == "user")
		return handle_post_kind_user(client, request, body);

	if(kind.empty() || kind == "guest")
		return handle_post_kind_guest(client, request, body);

	throw m::error
	{
		http::BAD_REQUEST, "M_UNKNOWN", "Unknown 'kind' of registration specified in query."
	};
}

resource register_resource
{
	"_matrix/client/r0/register",
	"Register for an account on this homeserver. (3.3.1)"
};

resource::method post
{
	register_resource, "POST", handle_post
};

mapi::header IRCD_MODULE
{
	"registers the resource 'client/register' to handle requests"
};
