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

static const auto description
{R"(
Retrieve all push rulesets for this user. Clients can "drill-down" on the rulesets by
suffixing a scope to this path e.g. /pushrules/global/. This will return a subset of this data
under the specified key e.g. the global key. (11.10.1.4.6)
)"};

resource pushrules
{
	"/_matrix/client/r0/pushrules",
	{
		description
	}
};

resource::response
get_pushrules(client &client, const resource::request &request)
try
{
	return resource::response
	{
		client, http::OK
	};
}
catch(...)
{
	throw;
}

resource::method get_method
{
	pushrules, "GET", get_pushrules
};

mapi::header IRCD_MODULE
{
	"registers the resource 'client/pushrules' to handle requests"
};
