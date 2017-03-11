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

db::handle logins
{
	"login"
};

resource login_resource
{
	"/_matrix/client/r0/login",
	"Authenticates the user by password, and issues an access token "
	"they can use to authorize themself in subsequent requests. (3.2.2)"
};

resource::method getter
{login_resource, "POST", [](client &client,
                            resource::request &request)
-> resource::response
{
	char head[256], body[256];
    static const auto headfmt
    {
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: %zu\r\n"
        "\r\n"
    };

	const json::obj in
	{
		json::doc{request.content}
	};

	const std::string user(in["user"]);
	if(!logins.has(user))
	{
		log::debug("User [%s] doesn't exist", user.data());
		write(logins, user, in.state);
	}
	else
	{
		log::debug("User [%s] found", user.data());
		logins(user, [](const string_view &foo)
		{
			printf("dox [%s]\n", std::string(foo.data(), foo.size()).data());
		});
	}

	json::obj out;
	out["user_id"] = in["user"];
	out["access_token"] = "{\"la\":\"abc123\"}";
	out["home_server"] = "wewt";

	const json::obj out2(out);

	char buf[256];
	const json::doc outd(serialize(out2, buf, buf + sizeof(buf)));
	for(const auto &member : outd)
		std::cout << member.first << " => " << member.second << std::endl;

	const size_t bodysz
	{
		print(body, sizeof(body), out)
	};

	const int headsz
	{
		snprintf(head, sizeof(head), headfmt, bodysz)
	};

	const const_buffers iov
	{
		{ head, size_t(headsz) },
		{ body, bodysz         },
	};

	client.sock->write(iov);
	return {};
}};

mapi::header IRCD_MODULE
{
	"registers the resource 'client/login' to handle requests"
};
