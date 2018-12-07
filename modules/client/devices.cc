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
	"Client 11.9 Device Management"
};

ircd::resource
devices_resource
{
	"/_matrix/client/r0/devices/",
	{
		"(11.9) Device Management",
		resource::DIRECTORY,
	}
};

static resource::response
get__devices_all(client &client,
                 const resource::request &request,
                 const m::room user_room)
{
	const m::room::state state
	{
		user_room
	};

	std::vector<std::string> devices;
	state.for_each("ircd.device", [&devices]
	(const m::event &event)
	{
		const auto &device_id
		{
			at<"state_key"_>(event)
		};

		devices.emplace_back(at<"content"_>(event));
	});

	const json::strung list
	{
		devices.data(), devices.data() + devices.size()
	};

	return resource::response
	{
		client, json::members
		{
			{ "devices", list }
		}
	};
}

resource::response
get__devices(client &client,
             const resource::request &request)
{
	const m::user::room user_room
	{
		request.user_id
	};

	if(request.parv.size() < 1)
		return get__devices_all(client, request, user_room);

	m::id::device::buf device_id
	{
		url::decode(device_id, request.parv[1])
	};

	user_room.get("ircd.device", device_id, [&]
	(const m::event &event)
	{
		resource::response
		{
			client, at<"content"_>(event)
		};
	});

	return {}; // responded from closure
}

resource::method
method_get
{
	devices_resource, "GET", get__devices,
	{
		method_get.REQUIRES_AUTH
	}
};

resource::response
put__devices(client &client,
              const resource::request &request)
{
	if(request.parv.size() < 1)
		throw m::NEED_MORE_PARAMS
		{
			"device_id required"
		};

	const m::user::room user_room
	{
		request.user_id
	};

	m::id::device::buf device_id
	{
		url::decode(device_id, request.parv[1])
	};

	user_room.get("ircd.device", device_id, [&]
	(const m::event &event)
	{
		const json::object &content
		{
			at<"content"_>(event)
		};

		//TODO: where is your Object.update()??
		throw m::UNSUPPORTED{};
	});

	send(user_room, request.user_id, "ircd.device", device_id, request.content);

	return resource::response
	{
		client, http::OK
	};
}

resource::method
method_put
{
	devices_resource, "PUT", put__devices,
	{
		method_put.REQUIRES_AUTH
	}
};

resource::response
delete__devices(client &client,
                const resource::request &request)
{
	if(request.parv.size() < 1)
		throw m::NEED_MORE_PARAMS
		{
			"device_id required"
		};

	const m::user::room user_room
	{
		request.user_id
	};

	m::id::device::buf device_id
	{
		url::decode(device_id, request.parv[1])
	};

	if(!user_room.has("ircd.device", device_id))
		throw m::NOT_FOUND
		{
			"User '%s' has no device with the ID '%s'",
			request.user_id,
			device_id
		};

	return resource::response
	{
		client, http::NOT_IMPLEMENTED
	};
}

resource::method
method_delete
{
	devices_resource, "DELETE", delete__devices,
	{
		method_delete.REQUIRES_AUTH
	}
};
