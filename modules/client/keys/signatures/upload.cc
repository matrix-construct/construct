// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m
{
	static resource::response post_keys_signatures_upload(client &, const resource::request &);
	extern resource::method keys_signatures_upload_post;
	extern resource keys_signatures_upload;
}

ircd::mapi::header
IRCD_MODULE
{
	"Client (undocumented) :Keys Signatures Upload"
};

decltype(ircd::m::keys_signatures_upload)
ircd::m::keys_signatures_upload
{
	"/_matrix/client/unstable/keys/signatures/upload",
	{
		"(undocumented) Keys Signatures Upload"
	}
};

decltype(ircd::m::keys_signatures_upload_post)
ircd::m::keys_signatures_upload_post
{
	keys_signatures_upload, "POST", post_keys_signatures_upload,
	{
		keys_signatures_upload_post.REQUIRES_AUTH
	}
};

ircd::m::resource::response
ircd::m::post_keys_signatures_upload(client &client,
                                     const resource::request &request)
{
	const auto src_dev
	{
		user::tokens::device(std::nothrow, request.access_token)
	};

	for(const auto &[user_id, device_keys_] : request)
	{
		if(!valid(m::id::USER, user_id))
			continue;

		const json::object device_keys
		{
			device_keys_
		};

		const user::room user_room
		{
			user::id{user_id}
		};

		for(const auto &[tgt_id, keys] : device_keys)
		{
			char state_key_buf[512];
			const string_view state_key
			{
				user::keys::make_sigs_state_key(state_key_buf, tgt_id, src_dev)
			};

			send(user_room, user_id, "ircd.keys.signatures", state_key, keys);
		}
	}

	return resource::response
	{
		client, json::members
		{
			{ "failures", json::empty_object }
		}
	};
}
