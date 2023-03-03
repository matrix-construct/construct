// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m::admin
{
	static resource::response handle_post(client &, const resource::request &);

	extern resource::method post_method;
	extern resource deactivate_resource;
};

ircd::mapi::header
IRCD_MODULE
{
	"Admin (undocumented) :Deactivate"
};

decltype(ircd::m::admin::deactivate_resource)
ircd::m::admin::deactivate_resource
{
	"/_synapse/admin/v1/deactivate/",
	{
		"(undocumented) Admin deactivate",
		resource::DIRECTORY
	}
};

decltype(ircd::m::admin::post_method)
ircd::m::admin::post_method
{
	deactivate_resource, "POST", handle_post,
	{
		post_method.REQUIRES_OPER
	}
};

ircd::m::resource::response
ircd::m::admin::handle_post(client &client,
                            const resource::request &request)
{
	if(request.parv.size() < 1)
		throw m::NEED_MORE_PARAMS
		{
			"user_id path parameter required"
		};

	user::id::buf user_id
	{
		url::decode(user_id, request.parv[0])
	};

	m::user user
	{
		user_id
	};

	if(!exists(user))
		throw m::NOT_FOUND
		{
			"%s is not a known user",
			string_view{user_id}
		};

	const event::id::buf event_id
	{
		active(user)?
			user.deactivate():
			event::id::buf{}
	};

	static const string_view id_server_unbind_result
	{
		"no-support"
	};

	return m::resource::response
	{
		client, json::members
		{
			{ "event_id",                event_id                },
			{ "id_server_unbind_result", id_server_unbind_result },
		}
	};
}
