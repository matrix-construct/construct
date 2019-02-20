// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

using namespace ircd;

mapi::header
IRCD_MODULE
{
	"Client 14.10.1.5 :Device Management"
};

ircd::resource
delete_devices_resource
{
	"/_matrix/client/r0/delete_devices/",
	{
		"14.10.1.5 :Device Management"
	}
};

ircd::resource::redirect::permanent
delete_devices_resource__unstable
{
	"/_matrix/client/unstable/delete_devices/",
	"/_matrix/client/r0/delete_devices/",
	{
		"14.10.1.5 :Device Management (redirect)"
	}
};

static resource::response
post__delete_devices(client &client,
                     const resource::request &request);

resource::method
method_post
{
	delete_devices_resource, "POST", post__delete_devices,
	{
		method_post.REQUIRES_AUTH
	}
};

resource::response
post__delete_devices(client &client,
                     const resource::request &request)
{
	const json::array &devices
	{
		request.at("devices")
	};

	const json::object &auth
	{
		request["auth"]
	};

	const string_view &type
	{
		auth.at("type")
	};

	const string_view &session
	{
		auth["session"]
	};

	for(const json::string &device_id : devices)
		m::device::del(request.user_id, device_id);

	return resource::response
	{
		client, http::OK
	};
}
