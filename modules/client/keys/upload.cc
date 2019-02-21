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
	"Client 14.11.5.2 :Key management API"
};

ircd::resource
upload_resource
{
	"/_matrix/client/r0/keys/upload",
	{
		"(14.11.5.2.1) Keys Upload",
		resource::DIRECTORY
	}
};

ircd::resource::redirect::permanent
upload_resource__unstable
{
	"/_matrix/client/unstable/keys/upload",
	"/_matrix/client/r0/keys/upload",
	{
		"(14.11.5.2.1) Keys Upload",
		resource::DIRECTORY
	}
};

static void
upload_device_keys(client &,
                   const resource::request &,
                   const m::device_keys &);

resource::response
post__keys_upload(client &client,
                  const resource::request &request)
{
	const m::user::room user_room
	{
		request.user_id
	};

	const json::object &device_keys
	{
		request["device_keys"]
	};

	if(!empty(device_keys))
		upload_device_keys(client, request, device_keys);

	const json::object &one_time_keys
	{
		request["one_time_keys"]
	};

	const int64_t curve25519_count{0};
	const int64_t signed_curve25519_count{0};
	const json::members one_time_key_counts
	{
		{ "curve25519",         curve25519_count        },
		{ "signed_curve25519",  signed_curve25519_count },
	};

	return resource::response
	{
		client, json::members
		{
			{ "one_time_key_counts", one_time_key_counts },
		}
	};
}

void
upload_device_keys(client &client,
                   const resource::request &request,
                   const m::device_keys &device_keys)
{
	if(at<"user_id"_>(device_keys) != request.user_id)
		throw m::FORBIDDEN
		{
			"client 14.11.5.2.1: device_keys.user_id: "
			"Must match the user_id used when logging in."
		};

	const m::device::id::buf device_id
	{
		m::user::get_device_from_access_token(request.access_token)
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

	m::device data;
	json::get<"device_id"_>(data) = device_id;
	json::get<"keys"_>(data) = request["device_keys"];
	m::device::set(request.user_id, data);
}

resource::method
method_post
{
	upload_resource, "POST", post__keys_upload,
	{
		method_post.REQUIRES_AUTH
	}
};
