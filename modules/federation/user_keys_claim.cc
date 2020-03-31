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

	m::resource::response::chunked response
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

	json::stack::object response_keys
	{
		top, "one_time_keys"
	};

	for(const auto &[user_id, devices] : one_time_keys)
	{
		const m::user::room user_room
		{
			user_id
		};

		json::stack::object response_user
		{
			response_keys, user_id
		};

		for(const auto &[device_id_, algorithm_] : json::object(devices))
		{
			const json::string &algorithm{algorithm_};
			const json::string &device_id{device_id_};
			const auto match{[&device_id]
			(const string_view &state_key)
			{
				return state_key == device_id;
			}};

			char buf[m::event::TYPE_MAX_SIZE];
			const string_view type{fmt::sprintf
			{
				buf, "ircd.device.one_time_key|%s",
				algorithm
			}};

			const m::room::type events
			{
				user_room, type, { -1UL, -1L }, true
			};

			json::stack::object response_device
			{
				response_user, device_id
			};

			events.for_each([&response_device, &match]
			(const string_view &type, const auto &, const m::event::idx &event_idx)
			{
				if(!m::query(std::nothrow, event_idx, "state_key", match))
					return true;

				m::get(std::nothrow, event_idx, "content", [&response_device, type]
				(const json::object &content)
				{
					const auto algorithm
					{
						split(type, '|').second
					};

					json::stack::member
					{
						response_device, algorithm, json::object
						{
							content[""] // device quirk
						}
					};
				});

				return false;
			});
		}
	}

	return {};
}
