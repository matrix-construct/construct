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

static void
upload_device_keys(client &,
                   const m::resource::request &,
                   const m::device::id &,
                   const m::device_keys &);

static void
upload_one_time_keys(client &,
                     const m::resource::request &,
                     const m::device::id &,
                     const json::object &);

static m::resource::response
post__keys_upload(client &client,
                  const m::resource::request &request);

mapi::header
IRCD_MODULE
{
	"Client 14.11.5.2 :Key management API"
};

ircd::m::resource
upload_resource
{
	"/_matrix/client/r0/keys/upload",
	{
		"(14.11.5.2.1) Keys Upload",
		resource::DIRECTORY
	}
};

ircd::m::resource
upload_resource__unstable
{
	"/_matrix/client/unstable/keys/upload",
	{
		"(14.11.5.2.1) Keys Upload",
		resource::DIRECTORY
	}
};

m::resource::method
method_post
{
	upload_resource, "POST", post__keys_upload,
	{
		method_post.REQUIRES_AUTH
	}
};

m::resource::method
method_post__unstable
{
	upload_resource__unstable, "POST", post__keys_upload,
	{
		method_post.REQUIRES_AUTH
	}
};

m::resource::response
post__keys_upload(client &client,
                  const m::resource::request &request)
{
	const m::user::room user_room
	{
		request.user_id
	};

	const json::object &device_keys
	{
		request["device_keys"]
	};

	const m::device::id::buf device_id
	{
		m::user::tokens::device(request.access_token)
	};

	if(!empty(device_keys))
		upload_device_keys(client, request, device_id, device_keys);

	const json::object &one_time_keys
	{
		request["one_time_keys"]
	};

	if(!empty(one_time_keys))
		upload_one_time_keys(client, request, device_id, one_time_keys);

	const unique_buffer<mutable_buffer> buf
	{
		32_KiB
	};

	json::stack out{buf};
	json::stack::object top{out};
	json::stack::object one_time_key_counts
	{
		top, "one_time_key_counts"
	};

	const auto counts
	{
		m::user::devices::count_one_time_keys(request.user_id, device_id)
	};

	for(const auto &[algorithm, count] : counts)
		json::stack::member
		{
			one_time_key_counts, algorithm, json::value{count}
		};

	one_time_key_counts.~object();
	top.~object();
	return m::resource::response
	{
		client, json::object
		{
			out.completed()
		}
	};
}

void
upload_one_time_keys(client &client,
                     const m::resource::request &request,
                     const m::device::id &device_id,
                     const json::object &one_time_keys)
{
	const m::user::devices devices
	{
		request.user_id
	};

	for(const auto &[ident, object] : one_time_keys)
	{
		const auto &[algorithm, name]
		{
			split(ident, ':')
		};

		if(empty(algorithm) || empty(name))
			continue;

		char state_key_buf[128]
		{
			"one_time_key|"
		};

		const string_view state_key
		{
			ircd::strlcat(state_key_buf, ident)
		};

		const auto set
		{
			devices.set(device_id, state_key, object)
		};

		log::debug
		{
			m::log, "Received one_time_key:%s for %s on %s",
			ident,
			string_view{device_id},
			string_view{request.user_id},
		};
	}
}

void
upload_device_keys(client &client,
                   const m::resource::request &request,
                   const m::device::id &device_id,
                   const m::device_keys &device_keys)
{
	if(at<"user_id"_>(device_keys) != request.user_id)
		throw m::FORBIDDEN
		{
			"client 14.11.5.2.1: device_keys.user_id: "
			"Must match the user_id used when logging in."
		};

	if(at<"device_id"_>(device_keys) != device_id)
		throw m::FORBIDDEN
		{
			"client 14.11.5.2.1: device_keys.device_id: "
			"Must match the device_id used when logging in."
		};

	const json::array &algorithms
	{
		at<"algorithms"_>(device_keys)
	};

	const json::object &keys
	{
		at<"keys"_>(device_keys)
	};

	const json::object &signatures
	{
		at<"signatures"_>(device_keys)
	};

	const m::user::devices devices
	{
		request.user_id
	};

	m::device data;
	json::get<"device_id"_>(data) = device_id;
	json::get<"keys"_>(data) = request["device_keys"];
	const auto set
	{
		devices.set(data)
	};
}
