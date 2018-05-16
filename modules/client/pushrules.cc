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
	"Client 11.12.1.5 :Push Rules API"
};

resource
pushrules
{
	"/_matrix/client/r0/pushrules",
	{
		"(11.12.1.5) Clients can retrieve, add, modify and remove push"
		" rules globally or per-device"
		,resource::DIRECTORY
	}
};

resource::response
get_pushrules(client &client, const resource::request &request)
try
{
	return resource::response
	{
		client, json::members
		{
			{ "global", json::members
			{
				{ "content",     json::array{} },
				{ "override",    json::array{} },
				{ "room",        json::array{} },
				{ "sender",      json::array{} },
				{ "underride",   json::array{} },
			}}
		}
	};
}
catch(...)
{
	throw;
}

resource::method
get_method
{
	pushrules, "GET", get_pushrules
};
