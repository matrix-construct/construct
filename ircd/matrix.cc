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

#include <ircd/lex_cast.h>

namespace ircd {
namespace m    {

} // namespace m
} // namespace ircd

void
ircd::test(const string_view &what)
try
{
	const auto tok(tokens(what, " "));
	const std::string method(tok.at(2));
	const std::string rcontent(tok.size() >= 5? tok.at(4) : string_view{});

	char url[256] {0};
	snprintf(url, sizeof(url), "/_matrix/client/%s", std::string(tok.at(3)).c_str());

	const std::string host{tok.at(0)};
	const auto port(lex_cast<uint16_t>(tok.at(1)));
	ircd::client client
	{
		host_port{host, port}
	};

	http::request
	{
		host, method, url, rcontent, write_closure(client),
		{
			{ "Content-Type"s, "application/json"s }
		}
	};

	char buf[4096];
	parse::buffer pb{buf};
	parse::context pc{pb, read_closure(client)};
	http::response
	{
		pc, nullptr, [&pc](const auto &head)
		{
			http::response::content content{pc, head};
			const json::doc cdoc{content};

			for(const auto &member : cdoc)
				std::cout << member.first << " --> " << member.second << std::endl;
		}
	};
}
catch(const std::exception &e)
{
	log::error("test: %s", e.what());
	throw;
}

ircd::m::error::error(const json::obj &obj)
:ircd::error{generate_skip}
{
	serialize(obj, begin(buf), end(buf));
}

ircd::m::error::error(const json::doc &doc)
:ircd::error{generate_skip}
{
	print(buf, sizeof(buf), doc);
}
