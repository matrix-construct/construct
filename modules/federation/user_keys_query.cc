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

m::resource
user_keys_query_resource
{
	"/_matrix/federation/v1/user/keys/query",
	{
		"federation user keys query",
	}
};

static void
_query_user_device(client &,
                   const m::resource::request &,
                   const m::user::devices &,
                   const string_view &device_id,
                   json::stack::object &out);

static void
_query_user_keys(client &,
                 const m::resource::request &,
                 json::stack &);

static void
_query_self_keys(client &,
                 const m::resource::request &,
                 json::stack &);

static void
_query_master_keys(client &,
                   const m::resource::request &,
                   json::stack &);

static void
_query_device_keys(client &,
                   const m::resource::request &,
                   json::stack &);

static m::resource::response
post__user_keys_query(client &client,
                      const m::resource::request &request);

m::resource::method
user_keys_query__post
{
	user_keys_query_resource, "POST", post__user_keys_query,
	{
		user_keys_query__post.VERIFY_ORIGIN
	}
};

m::resource::response
post__user_keys_query(client &client,
                      const m::resource::request &request)
{
	m::resource::response::chunked::json response
	{
		client, http::OK
	};

	_query_device_keys(client, request, response);
	_query_master_keys(client, request, response);
	_query_self_keys(client, request, response);
	if(my_host(request.node_id))
		_query_user_keys(client, request, response);

	return response;
}

void
_query_device_keys(client &client,
                   const m::resource::request &request,
                   json::stack &out)
{
	json::stack::object response_keys
	{
		out, "device_keys"
	};

	const json::object request_keys
	{
		request.at("device_keys")
	};

	for(const auto &[user_id_, device_ids_] : request_keys)
	{
		const m::user::id &user_id
		{
			user_id_
		};

		const json::array &device_ids
		{
			device_ids_
		};

		const m::user::devices devices
		{
			user_id
		};

		json::stack::object response_keys_user
		{
			response_keys, user_id
		};

		if(empty(device_ids))
			devices.for_each([&client, &request, &devices, &response_keys_user]
			(const auto &event_idx, const string_view &device_id)
			{
				_query_user_device(client, request, devices, device_id, response_keys_user);
				return true;
			});
		else
			for(const json::string device_id : device_ids)
				_query_user_device(client, request, devices, device_id, response_keys_user);
	}
}

void
_query_master_keys(client &client,
                   const m::resource::request &request,
                   json::stack &out)
{
	const json::object request_keys
	{
		request.at("device_keys")
	};

	json::stack::object response_keys
	{
		out, "master_keys"
	};

	for(const auto &[user_id_, device_ids_] : request_keys)
	{
		const m::user::id &user_id
		{
			user_id_
		};

		const m::user::keys keys
		{
			user_id
		};

		if(!keys.has_cross_master())
			continue;

		json::stack::object object
		{
			response_keys, user_id
		};

		keys.cross_master(object);
	}
}

void
_query_self_keys(client &client,
                 const m::resource::request &request,
                 json::stack &out)
{
	const json::object request_keys
	{
		request.at("device_keys")
	};

	json::stack::object response_keys
	{
		out, "self_signing_keys"
	};

	for(const auto &[user_id_, device_ids_] : request_keys)
	{
		const m::user::id &user_id
		{
			user_id_
		};

		const m::user::keys keys
		{
			user_id
		};

		if(!keys.has_cross_self())
			continue;

		json::stack::object object
		{
			response_keys, user_id
		};

		keys.cross_self(object);
	}
}

void
_query_user_keys(client &client,
                 const m::resource::request &request,
                 json::stack &out)
{
	const json::object request_keys
	{
		request.at("device_keys")
	};

	json::stack::object response_keys
	{
		out, "user_signing_keys"
	};

	for(const auto &[user_id_, device_ids_] : request_keys)
	{
		const m::user::id &user_id
		{
			user_id_
		};

		const m::user::keys keys
		{
			user_id
		};

		if(!keys.has_cross_user())
			continue;

		json::stack::object object
		{
			response_keys, user_id
		};

		keys.cross_user(object);
	}
}

void
_query_user_device(client &client,
                   const m::resource::request &request,
                   const m::user::devices &devices,
                   const string_view &device_id,
                   json::stack::object &out)
{
	const m::user::keys keys
	{
		devices.user
	};

	if(!keys.has_device(device_id))
		return;

	json::stack::object object
	{
		out, device_id
	};

	keys.device(object, device_id);

	devices.get(std::nothrow, device_id, "display_name", [&device_id, &object]
	(const auto &event_idx, const string_view &display_name)
	{
		json::stack::object non_hancock
		{
			object, "unsigned"
		};

		json::stack::member
		{
			non_hancock, "device_display_name", display_name
		};
	});
}
