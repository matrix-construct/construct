// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m::groups
{
	static resource::response handle_get(client &, const resource::request &);
	extern resource::method get_joined_groups;
	extern resource joined_groups_resource;
}

ircd::mapi::header
IRCD_MODULE
{
	"Client (unspecified) :Joined Groups",
};

decltype(ircd::m::groups::joined_groups_resource)
ircd::m::groups::joined_groups_resource
{
	"/_matrix/client/r0/joined_groups",
	{
		"(Unspecified/undocumented)"
	}
};

decltype(ircd::m::groups::get_joined_groups)
ircd::m::groups::get_joined_groups
{
	joined_groups_resource, "GET", handle_get,
	{
		get_joined_groups.REQUIRES_AUTH
	}
};

ircd::m::resource::response
ircd::m::groups::handle_get(client &client,
                            const resource::request &request)
{
	resource::response::chunked response
	{
		client, http::OK
	};

	json::stack out
	{
		response.buf, response.flusher()
	};

	json::stack::object top
	{
		out
	};

	json::stack::array groups
	{
		top, "groups"
	};

	const m::id::group group_ids[]
	{

	};

//	for(const auto &group_id : group_ids)
//		groups.append(group_id);

	return response;
}
