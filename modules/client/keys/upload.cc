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
                   const resource::request &,
                   const m::device::id &,
                   const m::device_keys &);

static resource::response
post__keys_upload(client &client,
                  const resource::request &request);

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

ircd::resource
upload_resource__unstable
{
	"/_matrix/client/unstable/keys/upload",
	{
		"(14.11.5.2.1) Keys Upload",
		resource::DIRECTORY
	}
};

resource::method
method_post
{
	upload_resource, "POST", post__keys_upload,
	{
		method_post.REQUIRES_AUTH
	}
};

resource::method
method_post__unstable
{
	upload_resource__unstable, "POST", post__keys_upload,
	{
		method_post.REQUIRES_AUTH
	}
};

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

	const m::device::id::buf device_id
	{
		m::user::get_device_from_access_token(request.access_token)
	};

	if(!empty(device_keys))
		upload_device_keys(client, request, device_id, device_keys);

	const json::object &one_time_keys
	{
		request["one_time_keys"]
	};

	//m::device::set(request.user_id, device_id, "one_time_keys", one_time_keys);

	size_t buf_est{64};
	std::map<string_view, long, std::less<>> counts;
	for(const auto &one_time_key : one_time_keys)
	{
		const auto name(split(one_time_key.first, ':'));
		const string_view &data(one_time_key.second);
		const string_view &algorithm(name.first);
		auto it(counts.lower_bound(algorithm));
		if(it == end(counts) || it->first != algorithm)
		{
			it = counts.emplace_hint(it, algorithm, 0L);
			buf_est += size(algorithm) + 32;
		}

		auto &count(it->second);
		// 0 or riot infinite-loops
		//++count;
	}

	const unique_buffer<mutable_buffer> buf
	{
		buf_est * 2
	};

	json::stack out{buf};
	{
		json::stack::object top{out};
		json::stack::object one_time_key_counts
		{
			top, "one_time_key_counts"
		};

		for(const auto &[algorithm, count] : counts)
			json::stack::member
			{
				one_time_key_counts, algorithm, json::value{count}
			};
	}

	return resource::response
	{
		client, json::object(out.completed())
	};
}

void
upload_device_keys(client &client,
                   const resource::request &request,
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

	m::device data;
	json::get<"device_id"_>(data) = device_id;
	json::get<"keys"_>(data) = request["device_keys"];
	m::device::set(request.user_id, data);
}
