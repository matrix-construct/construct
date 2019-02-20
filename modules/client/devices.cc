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

ircd::resource::redirect::permanent
devices_resource__unstable
{
	"/_matrix/client/unstable/devices/",
	"/_matrix/client/r0/devices/",
	{
		"(11.9) Device Management",
		resource::DIRECTORY,
	}
};

static void
_get_device(json::stack::object &obj,
            const m::user &user,
            const string_view &device_id)
{
	json::stack::member
	{
		obj, "device_id", device_id
	};

	m::device::get(std::nothrow, user, device_id, "display_name", [&obj]
	(const string_view &value)
	{
		json::stack::member
		{
			obj, "display_name", unquote(value)
		};
	});

	m::device::get(std::nothrow, user, device_id, "last_seen_ip", [&obj]
	(const string_view &value)
	{
		json::stack::member
		{
			obj, "last_seen_ip", unquote(value)
		};
	});

	m::device::get(std::nothrow, user, device_id, "last_seen_ts", [&obj]
	(const string_view &value)
	{
		json::stack::member
		{
			obj, "last_seen_ts", value
		};
	});
}

static resource::response
get__devices_all(client &client,
                 const resource::request &request,
                 const m::room user_room)
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

	json::stack::array devices
	{
		top, "devices"
	};

	m::device::for_each(request.user_id, [&request, &devices]
	(const string_view &device_id)
	{
		json::stack::object obj{devices};
		_get_device(obj, request.user_id, device_id);
		return true;
	});

	return {};
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
		url::decode(device_id, request.parv[0])
	};

	if(!m::device::has(request.user_id, device_id))
		throw m::NOT_FOUND
		{
			"Device ID '%s' not found", device_id
		};

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

	_get_device(top, request.user_id, device_id);
	return {};
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

	m::id::device::buf device_id
	{
		url::decode(device_id, request.parv[1])
	};

	m::device data{request.content};
	json::get<"device_id"_>(data) = device_id;

	m::device::set(request.user_id, data);

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

	m::id::device::buf device_id
	{
		url::decode(device_id, request.parv[1])
	};

	m::device::del(request.user_id, device_id);

	return resource::response
	{
		client, http::OK
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
