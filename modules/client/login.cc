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

resource login_resource
{
	"_matrix/client/r0/login",
	"Authenticates the user by password, and issues an access token "
	"they can use to authorize themself in subsequent requests. (3.2.2)"
};

const auto home_server
{
	// "The hostname of the homeserver on which the account has been registered."
	"cdc.z"
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

// Generate !accounts:host which is the MXID of the room where the members
// are the actual account registrations for this homeserver.
const m::id::room::buf accounts_room_id
{
	"accounts", home_server
};

// Handle to the accounts room
m::room accounts_room
{
	accounts_room_id
};

const m::room::events accounts_room_events
{
	accounts_room
};

resource::response
post_login_password(client &client,
                    const resource::request::object<body> &request)
{
	// Build a canonical MXID from a the user field
	const m::id::user::buf user_id
	{
		unquote(at<name::user>(request)), home_server
	};

	if(!user_id.valid() || user_id.host() != home_server)
		throw m::error
		{
			http::FORBIDDEN, "M_FORBIDDEN", "Access denied."
		};

	const auto &supplied_password
	{
		at<name::password>(request)
	};

	// Sets up the query to find the user_id in the accounts room
	const m::event::where::equal member_event
	{
		{ "type",        "m.room.member" },
		{ "state_key",    user_id        }
	};

	// Once the query finds the result this closure views the event in the
	// database and returns true if the login is authentic.
	const m::event::where::test correct_password{[&supplied_password]
	(const auto &event)
	{
		const json::object &content
		{
			json::val<m::name::content>(event)
		};

		const auto &membership
		{
			unquote(content.at("membership"))
		};

		if(membership != "join")
			return false;

		const auto &correct_password
		{
			content.get("password")
		};

		if(!correct_password)
			return false;

		if(supplied_password != correct_password)
			return false;

		return true;
	}};

	const auto query
	{
		member_event && correct_password
	};

	// The query to the database is made here. Know that this ircd::ctx
	// may suspend and global state may have changed after this call.
	if(!accounts_room_events.query(query))
		throw m::error
		{
			http::FORBIDDEN, "M_FORBIDDEN", "Access denied."
		};

	// Generate the access token
	static constexpr const auto token_len{127};
	static const auto token_dict{rand::dict::alpha};
	char token_buf[token_len + 1];
	const string_view access_token
	{
		rand::string(token_dict, token_len, token_buf, sizeof(token_buf))
	};

	// Log the user in by issuing an event in the accounts room containing
	// the generated token. When this call completes without throwing the
	// access_token will be committed and the user will be logged in.
	accounts_room.send(
	{
		{ "type",      "ircd.access_token"   },
		{ "sender",     user_id              },
		{ "state_key",  access_token         },
		{ "content",    json::members
		{
			{ "ip",      string(remote_addr(client)) },
			{ "device",  "unknown"                   },
		}}
	});

	// Send response to user
	return resource::response
	{
		client,
		{
			{ "user_id",        user_id        },
			{ "home_server",    home_server    },
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
		unquote(at<name::type>(request))
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

resource::method method_get
{
	login_resource, "GET", get_login
};

mapi::header IRCD_MODULE
{
	"registers the resource 'client/login' to handle requests"
};
