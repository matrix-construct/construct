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

mods::sym_ref<db::handle> accounts_ref
{
	"client_register",
	"accounts"
};

resource login_resource
{
	"/_matrix/client/r0/login",
	"Authenticates the user by password, and issues an access token "
	"they can use to authorize themself in subsequent requests. (3.2.2)"
};

resource::response
login(client &client, const resource::request &request)
try
{
	const auto user
	{
		unquote(request.at("user"))
	};

	const auto password
	{
		request.at("password")
	};

	const auto type
	{
		unquote(request["type"])
	};

	if(!type.empty() && type != "m.login.password")
	{
		throw m::error
		{
			"M_UNSUPPORTED", "Login type is not supported."
		};
	}

	db::handle &accounts(accounts_ref);
	accounts(user, [&password](const json::doc &user)
	{
		if(user["password"] != password)
			throw db::not_found();
	});

	const auto access_token(123456);
	const auto home_server("cdc.z");
	const auto device_id("ABCDEF");
	return resource::response
	{
		client,
		{
			{ "user_id",        user          },
			{ "access_token",   access_token  },
			{ "home_server",    home_server   },
			{ "device_id",      device_id     },
		}
	};
}
catch(const db::not_found &e)
{
	throw m::error
	{
		http::FORBIDDEN, "M_FORBIDDEN", "Access denied."
	};
}

resource::method post
{
	login_resource, "POST", login
};

mapi::header IRCD_MODULE
{
	"registers the resource 'client/login' to handle requests"
};
