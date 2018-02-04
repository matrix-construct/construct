// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

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
