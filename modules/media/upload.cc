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
	"media upload"
};

struct upload_resource
:resource
{
	using resource::resource;
}
upload_resource
{
	"/_matrix/media/r0/upload/", resource::opts
	{
		"media upload",
		resource::DIRECTORY
	}
};

resource::response
handle_post(client &client,
            const resource::request &request)
{
	auto filename
	{
		request.query["filename"]
	};

	return resource::response
	{
		client, http::OK
	};
}

resource::method method_post
{
	upload_resource, "POST", handle_post,
	{
		method_post.REQUIRES_AUTH
	}
};

resource::response
handle_put(client &client,
           const resource::request &request)
{
	auto filename
	{
		request.parv[0]
	};

	return resource::response
	{
		client, http::OK
	};
}
/*
resource::method method_put
{
	upload_resource, "PUT", handle_put,
	{
		method_put.REQUIRES_AUTH
	}
};
*/
