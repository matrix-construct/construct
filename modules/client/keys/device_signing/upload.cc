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
	extern std::string flows;
	static resource::response post_keys_device_signing_upload(client &, const resource::request &);
	extern resource::method keys_device_signing_upload_post;
	extern resource keys_device_signing_upload;
}

ircd::mapi::header
IRCD_MODULE
{
	"Client (undocumented) :Keys Device Signing Upload"
};

decltype(ircd::m::keys_device_signing_upload)
ircd::m::keys_device_signing_upload
{
	"/_matrix/client/unstable/keys/device_signing/upload",
	{
		"(undocumented) Keys Device Signing Upload"
	}
};

decltype(ircd::m::keys_device_signing_upload_post)
ircd::m::keys_device_signing_upload_post
{
	keys_device_signing_upload, "POST", post_keys_device_signing_upload,
	{
		keys_device_signing_upload_post.REQUIRES_AUTH
	}
};

ircd::m::resource::response
ircd::m::post_keys_device_signing_upload(client &client,
                                         const resource::request &request)
{
	const json::object &auth
	{
		request["auth"]
	};

	const json::object &master_key
	{
		request["master_key"]
	};

	const json::object &self_signing_key
	{
		request["self_signing_key"]
	};

	const json::object &user_signing_key
	{
		request["user_signing_key"]
	};

	const m::device::id::buf device_id
	{
		m::user::get_device_from_access_token(request.access_token)
	};

	const m::user::room user_room
	{
		request.user_id
	};

	if(empty(auth))
		return resource::response
		{
			client, http::UNAUTHORIZED, json::object{flows}
		};

	return resource::response
	{
		client, http::OK
	};
}

decltype(ircd::m::flows)
ircd::m::flows
{
	ircd::string(512 | SHRINK_TO_FIT, [](const mutable_buffer &buf)
	{
		json::stack out{buf};
		{
			json::stack::object top{out};
			json::stack::array flows{top, "flows"};
			json::stack::object flow{flows};
			json::stack::array stages{flow, "stages"};
			stages.append("m.login.password");
		}

		return out.completed();
	})
};
