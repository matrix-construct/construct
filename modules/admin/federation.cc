// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2023 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m::admin
{
	static resource::response handle_get_destinations(client &, const resource::request &);
	static resource::response handle_get(client &, const resource::request &);

	extern resource::method get_method;
	extern resource federation_resource;
};

ircd::mapi::header
IRCD_MODULE
{
	"Admin (undocumented) :Federation"
};

decltype(ircd::m::admin::federation_resource)
ircd::m::admin::federation_resource
{
	"/_synapse/admin/v1/federation/",
	{
		"(undocumented) Admin Federation",
		resource::DIRECTORY
	}
};

decltype(ircd::m::admin::get_method)
ircd::m::admin::get_method
{
	federation_resource, "GET", handle_get,
	{
		get_method.REQUIRES_OPER
	}
};

ircd::m::resource::response
ircd::m::admin::handle_get(client &client,
                           const resource::request &request)
{
	if(request.parv.size() < 1)
		throw m::NEED_MORE_PARAMS
		{
			"Command path parameter required"
		};

	const auto &cmd
	{
		request.parv[0]
	};

	if(cmd == "destinations")
		return handle_get_destinations(client, request);

	throw m::NOT_FOUND
	{
		"/admin/federation command not found"
	};
}

ircd::m::resource::response
ircd::m::admin::handle_get_destinations(client &client,
                                        const resource::request &request)
{
	return resource::response
	{
		client, http::NOT_IMPLEMENTED
	};
}
