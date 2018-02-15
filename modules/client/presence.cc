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
	"Client 11.6 :Presence"
};

ircd::resource
presence_resource
{
	"/_matrix/client/r0/presence/", resource::opts
	{
		"(11.6.2) Presence",
		resource::DIRECTORY,
	}
};

resource::response
put__presence(client &client,
              const resource::request &request)
{
	return resource::response
	{
		client, http::OK
	};
}

resource::method
method_put
{
	presence_resource, "PUT", put__presence,
	{
		method_put.REQUIRES_AUTH
	}
};
