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

mapi::header
IRCD_MODULE
{
	"Client 11.10.2 :End-to-End Encryption Keys"
};

ircd::resource
keys_upload_resource
{
	"/_matrix/client/unstable/keys/upload/", resource::opts
	{
		"(11.10.2.1) Keys Upload",
		resource::DIRECTORY,
	}
};

ircd::resource
keys_query_resource
{
	"/_matrix/client/unstable/keys/query", resource::opts
	{
		"(11.10.2.2) Keys Query",
	}
};

resource::response
post__keys_upload(client &client,
                  const resource::request &request)
{
	return resource::response
	{
		client, http::OK
	};
}

resource::method
upload_method_post
{
	keys_upload_resource, "POST", post__keys_upload,
	{
		upload_method_post.REQUIRES_AUTH
	}
};

resource::response
post__keys_query(client &client,
                 const resource::request &request)
{
	return resource::response
	{
		client, http::OK
	};
}

resource::method
query_method_post
{
	keys_query_resource, "POST", post__keys_query,
	{
		query_method_post.REQUIRES_AUTH
	}
};
