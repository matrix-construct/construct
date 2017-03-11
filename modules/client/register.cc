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

#include <ircd/socket.h>

//ircd::db::handle logins("login", db::opt::READ_ONLY);

ircd::mapi::header IRCD_MODULE
{
	"registers the resource 'client/register' to handle requests"
};

ircd::resource register_resource
{
	"/_matrix/client/r0/register",
	"Register for an account on this homeserver. (3.3.1)"
};

auto generate_access_token([] { });
auto generate_refresh_token([] { });
void valid_username(const ircd::string_view &s);
ircd::resource::response handle_post(ircd::client &client, ircd::resource::request &request);

template<class T>
struct in
{
};

template<class T>
struct out
{
};

template<class T>
struct query
{
};

struct string
{
};

struct object
{
};

struct boolean
{
};

template<>
struct in<string>
{
	in(const char *const &name, const std::function<void (const ircd::string_view &)> &valid = nullptr) {}
};

template<>
struct in<object>
{
	in(const char *const &name) {}
};

template<>
struct in<boolean>
{
	in(const char *const &name) {}
};

template<>
struct query<boolean>
{
	template<class A, class B> query(A&&, B&&) {}
};

template<>
struct out<string>
{
	template<class T> out(const char *const &name, T&&) {}
};

//
// Inputs
//
in<string> username        { "username",  valid_username                      };
in<string> password        { "password"                                       };
in<object> auth            { "auth"                                           };
in<string> session         { "auth.session"                                   };
in<string> type            { "auth.type"                                      };
in<boolean> bind_email     { "bind_email"                                     };

//
// Queries
//
ircd::db::handle users         { "users"                                         };
query<boolean> username_exists { users, username                                 };

//
// Commitments
//

//
// Outputs
//
out<string> user_id        { "user_id",        username                       };
out<string> home_server    { "home_server",    "pitcock's playground"         };
out<string> access_token   { "access_token",   generate_access_token          };
out<string> refresh_token  { "refresh_token",  generate_refresh_token         };

ircd::resource::method post
{
	register_resource, "POST", handle_post,
	{
		//&username,
		//&bind_email,
	}
};

ircd::resource::response
handle_post(ircd::client &client,
            ircd::resource::request &request)
{
	using namespace ircd;

	const json::doc in{request.content};

	json::obj hoo;
	hoo["kaa"] = "\"choo\"";

	json::obj foo;
	foo["yea"] = "\"pie\"";
	foo["hoo"] = &hoo;

	json::obj out;
	out["user_id"]         = in["username"];
	out["access_token"]    = "\"ABCDEFG\"";
	out["home_server"]     = "\"dereferenced\"";
	out["refresh_token"]   = "\"tokenizeddd\"";

	return { client, out };
}

void valid_username(const ircd::string_view &s)
{
	using namespace ircd;

	static conf::item<size_t> max_len
	{
		"client.register.username.max_len", 15
	};

	const json::obj err
	{json::doc{R"({
		"errcode": "M_INVALID_USERNAME",
		"error": "Username length exceeds maximum"
	})"s}};

	if(s.size() > max_len)
		throw m::error{err};
}
