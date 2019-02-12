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
	"Client 6 :Capabilities Negotiation"
};

resource
capabilities_resource
{
	"/_matrix/client/r0/capabilities",
	{
		"(6.1) Gets information about the server's supported feature "
		"set and other relevant capabilities. ",
	}
};

resource::response
get__capabilities(client &client, const resource::request &request)
{
	const bool m_change_password__enabled
	{
		mods::loaded("client_account")
	};

	return resource::response
	{
		client, json::members
		{
			{ "capabilities", json::members
			{
				{ "m.change_password", json::members
				{
					{ "enabled", m_change_password__enabled },
				}},
				{ "m.room_version", json::members
				{
					{ "default", "1" },
					{ "available", json::members
					{
						{ "1", "stable"    },
						{ "2", "unstable"  },
						{ "3", "unstable"  },
					}},
				}},
			}},
		},
	};
}

resource::method
get_method
{
	capabilities_resource, "GET", get__capabilities,
	{
		get_method.REQUIRES_AUTH |
		get_method.RATE_LIMITED
	}
};
