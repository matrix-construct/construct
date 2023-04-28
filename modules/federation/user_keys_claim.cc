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
post__user_keys_claim(client &client,
                      const m::resource::request &request);

mapi::header
IRCD_MODULE
{
	"Federation 22 :End-to-End Encryption"
};

m::resource
user_keys_claim_resource
{
	"/_matrix/federation/v1/user/keys/claim",
	{
		"Federation 22 :Claims one-time keys for use in pre-key messages.",
	}
};

m::resource::method
user_keys_claim__post
{
	user_keys_claim_resource, "POST", post__user_keys_claim,
	{
		user_keys_claim__post.VERIFY_ORIGIN
	}
};

m::resource::response
post__user_keys_claim(client &client,
                      const m::resource::request &request)
{
	const json::object &one_time_keys
	{
		request["one_time_keys"]
	};

	m::resource::response::chunked::json response
	{
		client, http::OK
	};

	json::stack::object response_keys
	{
		response, "one_time_keys"
	};

	for(const auto &[user_id_, devices] : one_time_keys)
	{
		const m::user::id user_id
		{
			user_id_
		};

		if(!m::exists(user_id))
			continue;

		const m::user::keys keys
		{
			user_id
		};

		json::stack::object response_user
		{
			response_keys, user_id
		};

		for(const auto &[device_id, algorithm_] : json::object(devices))
		{
			const json::string algorithm
			{
				algorithm_
			};

			json::stack::object response_device
			{
				response_user, device_id
			};

			keys.claim(response_device, device_id, algorithm);
		}
	}

	return response;
}
