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
	"/_matrix/client/r0/login",
	"Authenticates the user by password, and issues an access token "
	"they can use to authorize themself in subsequent requests. (3.2.2)"
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

resource::response
post_login_password(client &client,
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
		throw m::error
		{
			http::FORBIDDEN, "M_FORBIDDEN", "Access denied."
		};

	if(!user.is_active())
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

	// Log the user in by issuing an event in the sessions room containing
	// the generated token. When this call completes without throwing the
	// access_token will be committed and the user will be logged in.
	m::send(m::user::sessions, user_id, "ircd.access_token", access_token,
	{
		{ "ip",      string(remote(client)) },
		{ "device",  "unknown"              },
	});

	// Send response to user
	return resource::response
	{
		client,
		{
			{ "user_id",        user_id        },
			{ "home_server",    my_host()      },
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
		unquote(at<"type"_>(request))
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
