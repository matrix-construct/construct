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

using namespace ircd;

const std::string body
{[]{
	const json::doc object
	{R"(

	{"versions":["r0.0.1"]}

	)"};

	std::array<char, 128> body {0};
	const size_t size(0);
	//const size_t size(print(body.data(), body.size(), object));
	return std::string(body.data(), size);
}()};

const std::string header
{[]{
	const auto head
	{
		"HTTP/1.1 200 OK\r\n"
		"Content-Length: %zu\r\n"
		"\r\n"
	};

	std::array<char, 128> header;
	const size_t size(snprintf(header.data(), header.size(), head, body.size()));
	return std::string(header.data(), size);
}()};

resource versions_resource
{
	"/_matrix/client/r0/versions",
	"Gets the versions of the specification supported by the server (2.1)"
};

resource::method getter
{
	versions_resource, "GET", []
	(client &client, resource::request &request) -> resource::response
	{
		static const const_buffers iov
		{
			{ header.data(), header.size() },
			{ body.data(), body.size()     },
		};

		client.sock->write(iov);
		return {};
	}
};

mapi::header IRCD_MODULE
{
	"registers the resource 'client/versions' to handle requests"
};
