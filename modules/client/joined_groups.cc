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
	"Client (unspecified) :Joined Groups",
};

resource
joined_groups_resource
{
	"/_matrix/client/r0/joined_groups",
	{
		"Unspecified"
	}
};

resource::response
get__joined_groups(client &client,
                   const resource::request &request)
{
	return resource::response
	{
		client, json::members
		{
			//TODO: Unknown yet if "groups" is really a member array, just
			//TODO: a random guess which doesn't error riot.
			{ "groups", json::empty_array }
		}
	};
}

resource::method
get_method
{
	joined_groups_resource, "GET", get__joined_groups
};
