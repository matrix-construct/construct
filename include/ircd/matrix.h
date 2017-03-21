/*
 * charybdis: 21st Century IRC++d
 *
 * Copyright (C) 2016 Charybdis Development Team
 * Copyright (C) 2016 Jason Volk <jason@zemos.net>
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
 *
 */

#pragma once
#define HAVE_IRCD_MATRIX_H

namespace ircd {
namespace m    {

struct error
:http::error
{
	template<class... args> error(const http::code &, const string_view &errcode, const char *const &fmt, args&&...);
	template<class... args> error(const string_view &errcode, const char *const &fmt, args&&...);
	error(const http::code &, const json::doc &doc = {});
	error(const http::code &, const json::obj &obj);
};

struct member
{

};

template<const char *const &name>
struct mem
:member
{
	std::string value;

	operator const std::string &() const { return value; }

	mem(const json::obj &obj)
	:value{obj[name]}
	{}

	mem(const json::doc &doc)
	:value{doc[name]}
	{}

	mem() = default;

	friend std::ostream &operator<<(std::ostream &s, const mem &m)
	{
		s << m.value;
		return s;
	}
};

struct session
{
	static constexpr auto _user_id               { "user_id"         };
	static constexpr auto _access_token          { "access_token"    };
	static constexpr auto _home_server           { "home_server"     };
	static constexpr auto _device_id             { "device_id"       };

	mem<_user_id> user_id;
	mem<_access_token> access_token;
	mem<_home_server> home_server;
	mem<_device_id> device_id;

	session(const json::obj &obj)
	:user_id{obj}
	,access_token{obj}
	,home_server{obj}
	,device_id{obj}
	{}

	session() = default;
};

struct request
:json::obj
{
	struct quote;
	struct versions;
	struct sync;
	struct login;

	string_view method;
	string_view resource;
	string_view access_token;

	request(const string_view &method,
	        const string_view &resource,
	        std::initializer_list<json::obj::member> body = {})
	:json::obj{std::move(body)}
	,method{method}
	,resource{resource}
	{}

	request(const string_view &method,
	        const string_view &resource,
	        const json::doc &content)
	:json::obj{content}
	,method{method}
	,resource{resource}
	{}
};

struct request::sync
:request
{
/*
	bool full_state;
	string_view since;
	string_view filter;
	string_view set_presence;
	milliseconds timeout;
*/

	sync(std::initializer_list<json::obj::member> body = {})
	:request{"GET", "/_matrix/client/r0/sync", std::move(body)}
	{}
};

struct request::login
:request
{
	/*
	string_view user;
	string_view password;
	*/

	login(std::initializer_list<json::obj::member> body = {})
	:request{"POST", "/_matrix/client/r0/login", std::move(body)}
	{}
};

struct request::quote
:request
{
	quote(const string_view &method,
	      const string_view &resource,
	      const json::doc &content)
	:request{method, resource, content}
	{}
};

struct client
:ircd::client
{
	IRCD_EXCEPTION(ircd::error, error)

	std::unique_ptr<session> sess;

	// Synchronize server state
	void sync(request::sync &r);

	// Account login
	session login(request::login &);

	// Account registration
	void reg(const string_view &user,
	         const string_view &pass,
	         const string_view &type = "m.login.dummy");

	void quote(request::quote &);

	client(const host_port &);
};

using doc_closure = std::function<void (const json::doc &)>;
using arr_closure = std::function<void (const json::arr &)>;

struct request::versions
:request
{
	versions(client &, const doc_closure & = nullptr, std::initializer_list<json::obj::member> body = {});
};

} // namespace m
} // namespace ircd

inline
ircd::m::error::error(const http::code &c,
                      const json::obj &obj)
:http::error{c, std::string{obj}}
{}

inline
ircd::m::error::error(const http::code &c,
                      const json::doc &doc)
:http::error{c, std::string{doc}}
{}

template<class... args>
ircd::m::error::error(const string_view &errcode,
                      const char *const &fmt,
                      args&&... a)
:error
{
	http::BAD_REQUEST, errcode, fmt, std::forward<args>(a)...
}{}

template<class... args>
ircd::m::error::error(const http::code &status,
                      const string_view &errcode,
                      const char *const &fmt,
                      args&&... a)
:http::error
{
	status, [&]
	{
		char estr[256]; const auto estr_len
		{
			fmt::snprintf(estr, sizeof(estr), fmt, std::forward<args>(a)...)
		};

		return std::string
		{
			json::obj
			{
				{ "errcode",  errcode                      },
				{ "error",    string_view(estr, estr_len)  }
			}
		};
	}()
}{}
