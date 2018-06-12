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
	"Client 11.10.2 :End-to-End Encryption Keys"
};

ircd::resource
keys_upload_resource
{
	"/_matrix/client/unstable/keys/upload/", resource::opts
	{
		"(11.10.2.1) Keys Upload",
		resource::DIRECTORY,
	}
};

ircd::resource
keys_query_resource
{
	"/_matrix/client/unstable/keys/query", resource::opts
	{
		"(11.10.2.2) Keys Query",
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

	const json::object &one_time_keys
	{
		request["one_time_keys"]
	};

	if(!empty(device_keys))
	{
		const m::user::id &user_id
		{
			unquote(device_keys.at("user_id"))
		};

		if(user_id != request.user_id)
			throw m::FORBIDDEN
			{
				"client 11.10.2.1: device_keys.user_id: "
				"Must match the device ID used when logging in."
			};

		const m::id::device &device_id
		{
			unquote(device_keys.at("device_id"))
		};

		const json::array &algorithms
		{
			device_keys.at("algorithms")
		};

		const json::object &keys
		{
			device_keys.at("keys")
		};

		const json::object &signatures
		{
			device_keys.at("signatures")
		};
	}

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

resource::method
upload_method_post
{
	keys_upload_resource, "POST", post__keys_upload,
	{
		upload_method_post.REQUIRES_AUTH
	}
};

resource::response
post__keys_query(client &client,
                 const resource::request &request)
{
	return resource::response
	{
		client, http::OK
	};
}

resource::method
query_method_post
{
	keys_query_resource, "POST", post__keys_query,
	{
		query_method_post.REQUIRES_AUTH
	}
};
