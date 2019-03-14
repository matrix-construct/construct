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
	"Client 11.3 :Voice over IP"
};

resource
turnserver_resource
{
	"/_matrix/client/r0/voip/turnServer",
	{
		"(11.3.3) This API provides credentials for the client to use"
		" when initiating calls."
	}
};

static resource::response
get__turnserver(client &client,
                const resource::request &request);

resource::method
turnserver_get
{
	turnserver_resource, "GET", get__turnserver,
	{
		turnserver_get.REQUIRES_AUTH |
		turnserver_get.RATE_LIMITED
	}
};


conf::item<std::string>
turnserver_username
{
	{ "name",     "ircd.client.voip.turnserver.username" },
	{ "default",  string_view{}                          },
};

conf::item<std::string>
turnserver_password
{
	{ "name",     "ircd.client.voip.turnserver.password" },
	{ "default",  string_view{}                          },
};

conf::item<seconds>
turnserver_ttl
{
	{ "name",     "ircd.client.voip.turnserver.ttl" },
	{ "default",  86400                             },
};

// note: This has to be a fully valid JSON array of strings
conf::item<std::string>
turnserver_uris
{
	{ "name",     "ircd.client.voip.turnserver.uris" },
	{ "default",  json::empty_array                  },
};

resource::response
get__turnserver(client &client,
                const resource::request &request)
{
	return resource::response
	{
		client, json::members
		{
			{ "username",  string_view{turnserver_username}  },
			{ "password",  string_view{turnserver_password}  },
			{ "uris",      string_view{turnserver_uris}      },
			{ "ttl",       seconds(turnserver_ttl).count()   },
		}
	};
}
