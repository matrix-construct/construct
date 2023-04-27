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
			const auto &device_id{device_id_};
			const json::string &algorithm{algorithm_};
			const fmt::bsprintf<m::event::TYPE_MAX_SIZE> type
			{
				"ircd.device.one_time_key|%s",
				algorithm
			};

			const m::room::type events
			{
				user_room, type, { -1UL, -1L }, true
			};

			json::stack::object response_device
			{
				response_user, device_id
			};

			events.for_each([&user_room, &response_device, &device_id, &algorithm]
			(const string_view &type, const auto &, const m::event::idx &event_idx)
			{
				if(m::redacted(event_idx))
					return true;

				const bool match
				{
					m::query(std::nothrow, event_idx, "state_key", [&device_id]
					(const string_view &state_key) noexcept
					{
						return state_key == device_id;
					})
				};

				if(!match)
					return true;

				const bool fetched
				{
					m::get(std::nothrow, event_idx, "content", [&response_device, &algorithm]
					(const json::object &content)
					{
						json::stack::member
						{
							response_device, algorithm, json::object
							{
								content[""] // ircd.device.* quirk
							}
						};
					})
				};

				if(!fetched)
					return true;

				const auto event_id
				{
					m::event_id(event_idx)
				};

				const auto redact_id
				{
					m::redact(user_room, user_room.user, event_id, "claimed")
				};

				return false;
			});
		}
	}

	return response;
}
