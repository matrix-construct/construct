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
	"Federation 20 :Device Management"
};

resource
user_devices_resource
{
	"/_matrix/federation/v1/user/devices",
	{
		"federation user devices",
		resource::DIRECTORY,
	}
};

static resource::response
get__user_devices(client &client,
                  const resource::request &request);

resource::method
method_get
{
	user_devices_resource, "GET", get__user_devices,
	{
		method_get.VERIFY_ORIGIN
	}
};

resource::response
get__user_devices(client &client,
                  const resource::request &request)
{
	if(request.parv.size() < 1)
		throw m::NEED_MORE_PARAMS
		{
			"user_id path parameter required"
		};

	m::user::id::buf user_id
	{
		url::decode(user_id, request.parv[0])
	};

	return resource::response
	{
		client, http::NOT_FOUND
	};
}
