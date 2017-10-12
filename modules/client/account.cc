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
		"/_matrix/client/r0/account/deactivate",
		"Deactivate the user's account, removing all ability for the user to login again. (3.3.3)"
	};

	resource password
	{
		"/_matrix/client/r0/account/password",
		"Changes the password for an account on this homeserver. (3.3.4)"
	};

	using resource::resource;
}
account_resource
{
	"/_matrix/client/r0/account",
	"Account management (3.3)"
};

resource::response
account_password(client &client, const resource::request &request)
try
{
	const string_view &new_password
	{
		unquote(request.at("new_password"))
	};

	const string_view &type
	{
		unquote(request.at({"auth", "type"}))
	};

	if(type != "m.login.password")
		throw m::error
		{
			"M_UNSUPPORTED", "Login type is not supported."
		};

	const string_view &session
	{
		request[{"auth", "session"}]
	};

	m::user user
	{
		request.user_id
	};

	user.password(new_password);
	return resource::response
	{
		client, http::OK
	};
}
catch(const db::not_found &e)
{
	throw m::error
	{
		http::FORBIDDEN, "M_FORBIDDEN", "Access denied."
	};
}

resource::method post_password
{
	account_resource.password, "POST", account_password,
	{
		post_password.REQUIRES_AUTH
	}
};

resource::response
account_deactivate(client &client, const resource::request &request)
{
	const string_view &type
	{
		request.at({"auth", "type"})
	};

	const string_view &session
	{
		request[{"auth", "session"}]
	};

	m::user user
	{
		request.user_id
	};

	user.deactivate();

	return resource::response
	{
		client, json::members
		{
			{ "goodbye", "Thanks for visiting. Come back soon!" }
		}
	};
}

resource::method post_deactivate
{
	account_resource.deactivate, "POST", account_deactivate,
	{
		post_deactivate.REQUIRES_AUTH
	}
};

mapi::header IRCD_MODULE
{
	"registers the resource 'client/account' to handle requests"
};
