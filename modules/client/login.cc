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

namespace name
{
	extern constexpr auto password{"password"};
	extern constexpr auto medium{"medium"};
	extern constexpr auto type{"type"};
	extern constexpr auto user{"user"};
	extern constexpr auto address{"address"};
}

struct body
:json::tuple
<
	json::member<name::password, string_view>,
	json::member<name::medium, time_t>,
	json::member<name::type, string_view>,
	json::member<name::user, string_view>,
	json::member<name::address, string_view>
>
{
	using super_type::tuple;
};

// Generate !accounts:host which is the MXID of the room where the members
// are the actual account registrations themselves for this homeserver.
const m::id::room::buf accounts_room_id
{
	"accounts", home_server
};

// Handle to the accounts room
m::room accounts_room
{
	accounts_room_id
};

resource::response
post_login_password(client &client,
                    const resource::request &request,
                    const resource::request::body<body> &body)
{
	const m::id::user::buf user_id
	{
		unquote(at<name::user>(body)), home_server
	};

	if(!user_id.valid() || user_id.host() != home_server)
		throw m::error
		{
			http::FORBIDDEN, "M_FORBIDDEN", "Access denied."
		};

	const auto &supplied_password
	{
		at<name::password>(body)
	};

	const auto check{[&supplied_password]
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
			content.at("password")
		};

		if(supplied_password != correct_password)
			return false;

		return true;
	}};

	const m::events::where::equal query
	{
		{ "type",        "m.room.member" },
		{ "state_key",    user_id        }
	};

	if(!accounts_room.any(query, check))
		throw m::error
		{
			http::FORBIDDEN, "M_FORBIDDEN", "Access denied."
		};

	// Send response to user
	return resource::response
	{
		client,
		{
			{ "user_id",        string_view{user_id}    },
			{ "home_server",    home_server             },
//			{ "access_token",   access_token            },
		}
	};
}

resource::response
post_login(client &client, const resource::request &request)
{
	const resource::request::body<body> body
	{
		request
	};

	// x.x.x Required. The login type being used.
	// Currently only "m.login.password" is supported.
	const auto type
	{
		unquote(at<name::type>(body))
	};

	if(type == "m.login.password")
		return post_login_password(client, request, body);
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
	static const json::object flows
	{
		R"({"flows":[{"type":"m.login.password"}]})"
	};

	return resource::response
	{
		client, flows
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
