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
	"Identity Service 6 :Status Check"
};

resource
identity_resource
{
	"/_matrix/identity/api/v1",
	{
		"Identity 6.1 :Checks that an identity server is available at this"
		" API endpoint."
	}
};

static resource::response
handle_get(client &client,
           const resource::request &request)
{
	return resource::response
	{
		client, http::NOT_IMPLEMENTED
	};
}

resource::method
method_get
{
	identity_resource, "GET", handle_get
};
