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

#include "room.h"

using namespace ircd;

resource publicrooms_resource
{
	"_matrix/client/r0/publicRooms",
	"Lists the public rooms on the server. "
	"This API returns paginated responses. (7.5)"
};

resource::response
get_publicrooms(client &client, const resource::request &request)
{
	json::doc test
	{
		R"({"content":"hello","origin_server_ts":12345,"sender":"me"})"
	};

	m::event ev
	{
		"some content", 0, "some sender", "some type", "some unsigned", "statie"
	};

	json::for_each(ev, []
	(const auto &key, const auto &val)
	{
		std::cout << key << " => " << val << std::endl;
	});

	std::cout << std::endl;
	json::rfor_each(ev, []
	(const string_view &key, const auto &val)
	{
		std::cout << key << " => " << val << std::endl;
	});

	std::cout << std::endl;
	json::until(ev, []
	(const string_view &key, const auto &val)
	{
		std::cout << key << " => " << val << std::endl;
		return true;
	});

	std::cout << std::endl;
	json::runtil(ev, []
	(const string_view &key, const auto &val)
	{
		std::cout << key << " => " << val << std::endl;
		return true;
	});

	std::cout << std::endl;
	std::cout << index(ev, "origin_server_ts") << std::endl;
	std::cout << std::endl;

	return resource::response
	{
		client, json::index
		{
			{   }
		}
	};
}

resource::method post
{
	publicrooms_resource, "GET", get_publicrooms
};

mapi::header IRCD_MODULE
{
	"registers the resource 'client/publicrooms' to manage Matrix rooms"
};
