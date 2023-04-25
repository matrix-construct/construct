// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m::groups
{
	static resource::response handle_post(client &, const resource::request &);
	extern resource::method create_group_post;
	extern resource create_group_resource;
}

ircd::mapi::header
IRCD_MODULE
{
	"Client (unspecified) :Create Group",
};

decltype(ircd::m::groups::create_group_resource)
ircd::m::groups::create_group_resource
{
	"/_matrix/client/r0/create_group",
	{
		"(undocumented/unspecified) Create Group",
	}
};

decltype(ircd::m::groups::create_group_post)
ircd::m::groups::create_group_post
{
	create_group_resource, "POST", handle_post,
	{
		create_group_post.REQUIRES_AUTH
	}
};

ircd::m::resource::response
ircd::m::groups::handle_post(client &client,
                             const resource::request &request)
{
	const json::string &localpart
	{
		request["localpart"]
	};

	const json::object &profile
	{
		request["profile"]
	};

	const m::id::group::buf group_id
	{
		localpart, origin(my(request.user_id.host()))
	};

	return resource::response
	{
		client, http::OK, json::members
		{
			{ "group_id", group_id }
		},
	};
}
