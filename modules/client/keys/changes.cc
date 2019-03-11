// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

using namespace ircd;

static resource::response
get__keys_changes(client &client,
                  const resource::request &request);

mapi::header
IRCD_MODULE
{
	"Client 14.11.5.2 :Key management API"
};

ircd::resource
changes_resource
{
	"/_matrix/client/r0/keys/changes",
	{
		"(14.11.5.2.4) Keys changes",
	}
};

ircd::resource
changes_resource__unstable
{
	"/_matrix/client/unstable/keys/changes",
	{
		"(14.11.5.2.4) Keys changes",
	}
};

resource::method
method_get
{
	changes_resource, "GET", get__keys_changes,
	{
		method_get.REQUIRES_AUTH
	}
};

resource::method
method_get__unstable
{
	changes_resource__unstable, "GET", get__keys_changes,
	{
		method_get.REQUIRES_AUTH
	}
};

resource::response
get__keys_changes(client &client,
                  const resource::request &request)
{
	return resource::response
	{
		client, http::OK
	};
}
