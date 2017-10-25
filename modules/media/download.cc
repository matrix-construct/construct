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

mapi::header IRCD_MODULE
{
	"media download"
};

struct send
:resource
{
	using resource::resource;
}
download_resource
{
	"/_matrix/media/r0/download/", resource::opts
	{
		resource::DIRECTORY,
		"media download"
	}
};

resource::response
handle_get(client &client,
           const resource::request &request)
{
	if(request.parv.size() < 2)
		throw http::error
		{
			http::MULTIPLE_CHOICES, "/ download / domain / file"
		};

	const auto &domain
	{
		request.parv[0]
	};

	const auto &file
	{
		request.parv[1]
	};

	const fmt::snstringf path
	{
		1024,
		"/home/jason/.synapse/media_store/local_content/%s/%s/%s",
		file.substr(0, 2),
		file.substr(2, 2),
		file.substr(4, file.size() - 4)
	};

	const auto data
	{
		fs::read(path)
	};

	return resource::response
	{
		client, string_view{data}, "image/jpg"
	};
}

resource::method method_put
{
	download_resource, "GET", handle_get,
	{
	}
};
