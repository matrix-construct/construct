// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include "groups.h"

namespace ircd::m::groups
{
	static resource::response handle_get(client &, const resource::request &);
	extern resource::method groups_get;

	extern resource groups_resource;
}

ircd::mapi::header
IRCD_MODULE
{
	"Client (unspecified) :Groups",
};

decltype(ircd::m::groups::groups_resource)
ircd::m::groups::groups_resource
{
	"/_matrix/client/r0/groups",
	{
		"(undocumented/unspecified) Groups",
		resource::DIRECTORY
	}
};

decltype(ircd::m::groups::groups_get)
ircd::m::groups::groups_get
{
	groups_resource, "GET", handle_get,
	{
		groups_get.REQUIRES_AUTH
	}
};

ircd::m::resource::response
ircd::m::groups::handle_get(client &client,
                            const resource::request &request)
{

	return resource::response
	{
		client, http::NOT_FOUND
	};
}
