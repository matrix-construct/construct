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
	"Federation 2.2 :Version"
};

resource
version_resource
{
	"/_matrix/federation/v1/version",
	{
		"(2.2) Get the implementation name and version of this homeserver",
	}
};

resource::response
get__version(client &client,
             const resource::request &request)
{
	const json::members server
	{
		{ "name",     ircd::info::name    },
		{ "version",  ircd::info::version },
	};

	return resource::response
	{
		client, json::members
		{
			{ "server", server }
		}
	};
}

resource::method
method_get
{
	version_resource, "GET", get__version
};
