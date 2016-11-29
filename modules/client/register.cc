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

using namespace ircd;

mapi::header IRCD_MODULE
{
	"registers the resource 'client/register' to handle requests"
};

resource register_resource
{
	"/_matrix/client/r0/register",
	"Register for an account on this homeserver. (3.3.1)"
};

void valid_username(const string_view &s);
void handle_post(client &client, resource::request &request, resource::response &response);

resource::member username     { "username",    json::STRING,  valid_username };
resource::member bind_email   { "bind_email",  json::LITERAL                 };
resource::method on_post
{
	register_resource, "POST", handle_post,
	{
		&username,
		&bind_email,
	}
};

void handle_post(client &client,
                 resource::request &request,
                 resource::response &response)
{
	const json::doc in{request.content};

	char head[256], body[25];
    static const auto headfmt
    {
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: %zu\r\n"
        "\r\n"
    };

	std::cout << request.content << std::endl;
	//std::cout << in << std::endl;

//	write(client, in);
}

void valid_username(const string_view &s)
{
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
