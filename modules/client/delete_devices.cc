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

static m::resource::response
post__delete_devices(client &client,
                     const m::resource::request &request);

extern const std::string flows;

mapi::header
IRCD_MODULE
{
	"Client 14.10.1.5 :Device Management"
};

ircd::m::resource
delete_devices_resource
{
	"/_matrix/client/r0/delete_devices/",
	{
		"14.10.1.5 :Device Management"
	}
};

m::resource::method
method_post
{
	delete_devices_resource, "POST", post__delete_devices,
	{
		method_post.REQUIRES_AUTH
	}
};

m::resource::response
post__delete_devices(client &client,
                     const m::resource::request &request)
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
		return m::resource::response
		{
			client, http::UNAUTHORIZED, json::object{flows}
		};

	const json::string &password{auth["password"]};
	if(!m::user(request.user_id).is_password(password))
		throw m::ACCESS_DENIED
		{
			"Incorrect password."
		};

	const m::user::devices user_devices
	{
		request.user_id
	};

	const m::user::tokens access_tokens
	{
		request.user_id
	};

	size_t revoked(0);
	for(const json::string device_id : devices)
		revoked += access_tokens.del_by_device(device_id, "device deleted");

	size_t deleted(0);
	for(const json::string device_id : devices)
		deleted += user_devices.del(device_id);

	return m::resource::response
	{
		client, json::members
		{
			{ "deleted", long(deleted) },
			{ "revoked", long(revoked) },
		}
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
