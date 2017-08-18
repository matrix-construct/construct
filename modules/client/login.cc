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

using object = db::object<m::db::accounts>;
template<class T = string_view> using value = db::value<m::db::accounts, T>;

resource::response
post_login_password(client &client, const resource::request &request)
{
	const auto user
	{
		// "The fully qualified user ID or just local part of the user ID, to log in."
		unquote(request.at("user"))
	};

    char user_id[m::USER_ID_BUFSIZE]; m::id
    {
        user, home_server, user_id
    };

	const auto password
	{
		// "Required. The user's password."
		request.at("password")
	};

    value<> password_text("password.text", user_id);
	if(password_text != password)
		throw m::error
		{
			http::FORBIDDEN, "M_FORBIDDEN", "Access denied."
		};

	// "An access token for the account. This access token can then be used to "
	// "authorize other requests. The access token may expire at some point, and if "
	// "so, it SHOULD come with a refresh_token. There is no specific error message to "
	// "indicate that a request has failed because an access token has expired; "
	// "instead, if a client has reason to believe its access token is valid, and "
	// "it receives an auth error, they should attempt to refresh for a new token "
	// "on failure, and retry the request with the new token."
	value<> access_token_text{"access_token.text", user_id};

	 // Generate access token
	char access_token[m::ACCESS_TOKEN_BUFSIZE];
	m::access_token_generate(access_token, sizeof(access_token));

	// Write access token to database
	access_token_text = access_token;
	ircd::resource::tokens.emplace(access_token, &client); //TODO: XXX
	value<> token_to_user_id{"token", access_token_text};
	token_to_user_id = user_id;

	// Send response to user
	return resource::response
	{
		client,
		{
			{ "user_id",        user_id       },
			{ "access_token",   access_token  },
			{ "home_server",    home_server   },
		}
	};
}

resource::response
post_login_token(client &client, const resource::request &request)
{
	const auto token
	{
		unquote(request.at("token"))
	};

	const value<> user_id
	{
		"token", token
	};

	if(!user_id)
		throw m::error
		{
			http::FORBIDDEN, "M_FORBIDDEN", "Access denied."
		};

	return resource::response
	{
		client,
		{
			{ "user_id",        string_view(user_id) },
			{ "access_token",   token                },
			{ "home_server",    home_server          },
		}
	};
}

resource::response
post_login(client &client, const resource::request &request)
{
	const auto type
	{
		// "Required. The login type being used. Currently only "m.login.password" is supported."
		unquote(request.at("type"))
	};

	if(type == "m.login.password")
		return post_login_password(client, request);
	else if(type == "m.login.token")
		return post_login_token(client, request);
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
