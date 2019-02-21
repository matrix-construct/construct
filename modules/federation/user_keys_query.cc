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
	"Federation 21 :End-to-End Encryption"
};

resource
user_keys_query_resource
{
	"/_matrix/federation/v1/user/keys/query",
	{
		"federation user keys query",
	}
};

static bool
_query_user_device(client &client,
                   const resource::request &request,
                   const m::user::id &user_id,
                   const string_view &device_id,
                   json::stack::object &out);

static resource::response
post__user_keys_query(client &client,
                      const resource::request &request);

resource::method
user_keys_query__post
{
	user_keys_query_resource, "POST", post__user_keys_query,
	{
		user_keys_query__post.VERIFY_ORIGIN
	}
};

resource::response
post__user_keys_query(client &client,
                      const resource::request &request)
{
	const json::object &request_keys
	{
		request.at("device_keys")
	};

	resource::response::chunked response
	{
		client, http::OK
	};

	json::stack out
	{
		response.buf, response.flusher()
	};

	json::stack::object top{out};
	json::stack::object response_keys
	{
		top, "device_keys"
	};

	for(const auto &m : request_keys)
	{
		const m::user::id &user_id{m.first};
		const json::array &device_ids{m.second};
		json::stack::object response_keys_user
		{
			response_keys, user_id
		};

		if(empty(device_ids))
			m::device::for_each(user_id, [&client, &request, &user_id, &response_keys_user]
			(const string_view &device_id)
			{
				_query_user_device(client, request, user_id, device_id, response_keys_user);
				return true;
			});
		else
			for(const json::string &device_id : device_ids)
				_query_user_device(client, request, user_id, device_id, response_keys_user);
	}

	return response;
}

bool
_query_user_device(client &client,
                   const resource::request &request,
                   const m::user::id &user_id,
                   const string_view &device_id,
                   json::stack::object &out)
{
	return m::device::get(std::nothrow, user_id, device_id, "keys", [&device_id, &out]
	(const json::object &device_keys)
	{
		json::stack::member
		{
			out, device_id, device_keys
		};
	});
}
