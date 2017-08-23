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

#include "account.h"

using namespace ircd;

using object = db::object<account>;
template<class T = string_view> using value = db::value<T, account>;

const auto home_server
{
	// "The hostname of the homeserver on which the account has been registered."
	"cdc.z"
};

resource::response
handle_post(client &client,
            resource::request &request)
{
	const auto kind
	{
		request.query["kind"]
	};

	if(kind.empty() || kind == "guest")
	{
		char user_id[m::USER_ID_BUFSIZE]; m::id
		{
			"randy12345", home_server, user_id
		};
		char access_token[m::ACCESS_TOKEN_BUFSIZE];
		m::access_token_generate(access_token, sizeof(access_token));
    	ircd::resource::tokens.emplace(access_token, &client); //TODO: XXX
		value<> token_to_user_id{"token", access_token};
		token_to_user_id = user_id;

		return resource::response
		{
			client, http::CREATED, json::index
			{
				{ "access_token",    access_token   },
				{ "home_server",     home_server    },
				{ "user_id",         user_id        },
			}
		};
	}

	const auto type
	{
		// "Required. The login type that the client is attempting to complete."
		unquote(request.at("auth.type"))
	};

	if(type != "m.login.dummy")
		throw m::error
		{
			"M_UNSUPPORTED", "Registration '%s' not supported.", type
		};

	const auto username
	{
		// "The local part of the desired Matrix ID. If omitted, the homeserver MUST "
		// "generate a Matrix ID local part."
		unquote(request.get("username"))
	};

	if(!username.empty() && !m::username_valid(username))
		throw m::error
		{
			"M_INVALID_USERNAME", "The desired user ID is not a valid user name."
		};

	const auto password
	{
		// "Required. The desired password for the account."
		request.at("password")
	};

	const auto bind_email
	{
		// "If true, the server binds the email used for authentication to the "
		// "Matrix ID with the ID Server."
		request.get<bool>("bind_email", false)
	};

	// Generate fully qualified user id and randomize username if missing
	char user_id[m::USER_ID_BUFSIZE]; m::id
	{
		username, home_server, user_id
	};

	// Atomic commitment to registration
	value<time_t> registered{"registered", user_id};
	{
		time_t expected(0); // unregistered == empty == 0
		if(!registered.compare_exchange(expected, time(nullptr)))
			throw m::error
			{
				http::CONFLICT, "M_USER_IN_USE", "The desired user ID is already taken."
			};
	}

	// "An access token for the account. This access token can then be used to "
	// "authorize other requests. The access token may expire at some point, and if "
	// "so, it SHOULD come with a refresh_token. There is no specific error message to "
	// "indicate that a request has failed because an access token has expired; "
	// "instead, if a client has reason to believe its access token is valid, and "
	// "it receives an auth error, they should attempt to refresh for a new token "
	// "on failure, and retry the request with the new token."
	value<> access_token_text{"access_token.text", user_id};

	// Prepare to store password
	value<> password_text("password.text", user_id);

	// Generate access token
	char access_token[m::ACCESS_TOKEN_BUFSIZE];
	m::access_token_generate(access_token, sizeof(access_token));
    ircd::resource::tokens.emplace(access_token, &client); //TODO: XXX

	// Batch transaction to database
	db::write
	({
		{ db::SET,  password_text,      password      },
		{ db::SET,  access_token_text,  access_token  },
	});

	// Send response to user
	return resource::response
	{
		client, http::CREATED, json::index
		{
			{ "access_token",    access_token   },
			{ "home_server",     home_server    },
			{ "user_id",         user_id        },
		}
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
