/*
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
 */

namespace ircd {
namespace m    {

db::handle accounts;

} // namespace m
} // namespace ircd

ircd::m::client::client(const host_port &host_port)
:ircd::client{host_port}
{
}

ircd::m::request::versions::versions(client &client,
                                     const doc_closure &closure,
                                     std::initializer_list<json::obj::member> body)
:request
{
	"GET", "/_matrix/client/versions", std::move(body)
}
{
	const auto handle([&](const http::code &code,
	                      const json::doc &doc)
	{
		switch(code)
		{
			case http::OK:
				if(closure)
					closure(doc);

				break;

			default:
				throw m::error(code, doc);
		}
	});

	char buf[1024];
	parse::buffer pb{buf};
	parse::capstan pc{pb, read_closure(client)};
	const auto handler([&pc, &handle]
	(const http::response::head &head)
	{
		const http::response::content content{pc, head};
		const auto status(http::status(head.status));
		const json::doc doc{content};
		handle(status, doc);
	});

	http::request
	{
		host(remote_addr(client)), method, resource, std::string(*this), write_closure(client),
		{
			{ "Content-Type"s, "application/json"s }
		}
	};

	http::response
	{
		pc, nullptr, handler
	};
}

void
ircd::m::client::reg(const string_view &user,
                     const string_view &pass,
                     const string_view &type)
{
	const json::obj auth
	{
		{ "type", type }
	};

	const json::obj obj
	{
		{ "password",  pass   },
		{ "username",  user   },
		{ "auth",      &auth  },
	};

	const auto handle([&](const http::code &code,
	                      const json::doc &doc)
	{
		switch(code)
		{
			case http::OK:
				std::cout << "OK!" << std::endl;
				std::cout << doc << std::endl;
				break;

			default:
				throw m::error(code, doc);
		}
	});

	char buf[4096];
	parse::buffer pb{buf};
	parse::capstan pc{pb, read_closure(*this)};
	const auto handler([&pc, &handle]
	(const http::response::head &head)
	{
		const http::response::content content{pc, head};
		const auto status(http::status(head.status));
		const json::doc doc{content};
		handle(status, doc);
	});

	http::request
	{
		host(remote_addr(*this)), "POST"s, "/_matrix/client/r0/register"s, std::string(obj), write_closure(*this),
		{
			{ "Content-Type"s, "application/json"s }
		}
	};

	http::response
	{
		pc, nullptr, handler
	};


}

ircd::m::session
ircd::m::client::login(request::login &r)
{
	session ret;

	const auto handle([&](const http::code &code,
	                      const json::doc &doc)
	{
		switch(code)
		{
			case http::OK:
				ret = json::obj{doc};
				this->sess.reset(new session{doc});
				break;

			default:
				throw m::error(code, doc);
		}
	});

	char buf[4096];
	parse::buffer pb{buf};
	parse::capstan pc{pb, read_closure(*this)};
	const auto handler([&pc, &handle]
	(const http::response::head &head)
	{
		const http::response::content content{pc, head};
		const auto status(http::status(head.status));
		const json::doc doc{content};
		handle(status, doc);
	});

	http::request
	{
		host(remote_addr(*this)), r.method, r.resource, std::string(r), write_closure(*this),
		{
			{ "Content-Type"s, "application/json"s }
		}
	};

	http::response
	{
		pc, nullptr, handler
	};

	return ret;
}

void
ircd::m::client::sync(request::sync &r)
{
	if(!sess)
		throw error("No active session");

	static const auto urlfmt
	{
		"%s?access_token=%s"
	};

	char url[1024]; const auto urllen
	{
		fmt::snprintf(url, sizeof(url), urlfmt, r.resource, string(sess->access_token))
	};

	http::request
	{
		host(remote_addr(*this)), r.method, url, {}, write_closure(*this),
		{
			{ "Content-Type"s, "application/json"s }
		}
	};

	char buf[4096];
	parse::buffer pb{buf};
	parse::capstan pc{pb, read_closure(*this)};
	http::response
	{
		pc, nullptr, [this, &pc](const auto &head)
		{
			http::response::content content
			{
				pc, head
			};

			switch(http::status(head.status))
			{
				case http::OK:
				{
					const json::obj d(content, true);
					const json::arr a(d["account_data.events[0]"]);
					std::cout << a << std::endl;
					break;
				}

				default:
					throw m::error(http::status(head.status), json::doc(content));
			}
		}
	};
}

void
ircd::m::client::quote(request::quote &r)
{
}
