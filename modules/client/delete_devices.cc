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

static resource::response
post__delete_devices(client &client,
                     const resource::request &request);

extern const std::string flows;

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

ircd::resource
delete_devices_resource__unstable
{
	"/_matrix/client/unstable/delete_devices/",
	{
		"14.10.1.5 :Device Management (redirect)"
	}
};

resource::method
method_post
{
	delete_devices_resource, "POST", post__delete_devices,
	{
		method_post.REQUIRES_AUTH
	}
};

resource::method
method_post__unstable
{
	delete_devices_resource__unstable, "POST", post__delete_devices,
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

	// 14.10.2 Security considerations
	const json::string &type{auth["type"]};
	if(type != "m.login.password")
		return resource::response
		{
			client, http::UNAUTHORIZED, json::object{flows}
		};

	const json::string &password{auth["password"]};
	if(!m::user(request.user_id).is_password(password))
		throw m::ACCESS_DENIED
		{
			"Incorrect password."
		};

	for(const json::string &device_id : devices)
		m::device::del(request.user_id, device_id);

	return resource::response
	{
		client, http::OK
	};
}

const std::string
flows
{
	ircd::string(512 | SHRINK_TO_FIT, [](const mutable_buffer &buf)
	{
		json::stack out{buf};
		{
			json::stack::object top{out};
			json::stack::array flows{top, "flows"};
			json::stack::object flow{flows};
			json::stack::array stages{flow, "stages"};
			stages.append("m.login.password");
		}

		return out.completed();
	})
};
