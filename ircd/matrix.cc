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
#include <ircd/socket.h>

namespace ircd {
namespace m    {

} // namespace m
} // namespace ircd

void
ircd::test(const string_view &what)
try
{
	http::client client{host_port{"matrix.org", 8448}};

	static const string_view request
	{
		"GET /_matrix/client/versions HTTP/1.1\r\nHost: matrix.org\r\n\r\n"
	};

	auto &sock(*client.sock);
	write(sock, request);

	char buf[4096];
	parse::buffer pb(buf);
	parse::context pc(pb, [&sock]
	(char *&read, char *const &stop)
	{
		read += sock.read_some(mutable_buffers{{read, stop}});
	});

	const http::response::head header{pc};
	const http::response::body content{pc, header};
	for(const auto &member : json::doc(content))
		std::cout << member.first << " --> " << member.second << std::endl;
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
