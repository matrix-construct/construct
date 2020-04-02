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
	const m::device::id::buf device_id
	{
		m::user::tokens::device(request.access_token)
	};

	const m::user::room user_room
	{
		request.user_id
	};

	for(const auto &[_user_id, devices_keys_] : request)
	{
		if(!valid(m::id::USER, _user_id))
			continue;

		if(_user_id != request.user_id)
			throw m::ACCESS_DENIED
			{
				"Uploading for user %s by %s not allowed or supported",
				_user_id,
				string_view{request.user_id},
			};

		const m::user::id user_id
		{
			_user_id
		};

		const m::user::devices devices
		{
			user_id
		};

		const json::object &devices_keys
		{
			devices_keys_
		};

		for(const auto &[_device_id, device_keys_] : devices_keys)
		{
			const m::device_keys device_keys
			{
				device_keys_
			};

			if(json::get<"device_id"_>(device_keys) != _device_id)
				throw m::BAD_REQUEST
				{
					"device_id '%s' does not match object property name '%s'",
					json::get<"device_id"_>(device_keys),
					_device_id,
				};

			if((false) && _device_id != device_id) // is this the "cross-sign?" gotta find out!
				throw m::ACCESS_DENIED
				{
					"device_id '%s' does not match your current device_id '%s'",
					_device_id,
					string_view{device_id},
				};

			const bool set
			{
				devices.set(_device_id, "signatures", device_keys_)
			};
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
