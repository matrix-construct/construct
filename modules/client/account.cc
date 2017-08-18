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

struct account
:resource
{
	resource deactivate
	{
		"_matrix/client/r0/account/deactivate",
		"Deactivate the user's account, removing all ability for the user to login again. (3.3.3)"
	};

	resource password
	{
		"_matrix/client/r0/account/password",
		"Changes the password for an account on this homeserver. (3.3.4)"
	};

	using resource::resource;
}
account_resource
{
	"_matrix/client/r0/account",
	"Account management (3.3)"
};

resource::response
account_password(client &client, const resource::request &request)
try
{
	const auto new_password
	{
		request.at("new_password")
	};

	const auto type
	{
		unquote(request.at("auth.type"))
	};

	const auto session
	{
		request["auth.session"]
	};

	if(!type.empty() && type != "m.login.token")
	{
		throw m::error
		{
			"M_UNSUPPORTED", "Login type is not supported."
		};
	}

	//db::handle hand(account, "foo");
	//database::snapshot snap(hand);

/*
	account(db::MERGE, "jzk", std::string
	{
		json::index
		{{
			"password",
			{
				{ "plaintext", new_password }
			}
		}}
	});
*/
	return resource::response
	{
		client
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
	account_resource.password, "POST", account_password
};

mapi::header IRCD_MODULE
{
	"registers the resource 'client/login' to handle requests"
};
