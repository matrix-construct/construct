// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m::widget
{
	static resource::response handle_post(client &, const resource::request &);
	extern resource::method register_post;
	extern resource register_resource;
}

ircd::mapi::header
IRCD_MODULE
{
	"Widget 1.0 :Register",
};

decltype(ircd::m::widget::register_resource)
ircd::m::widget::register_resource
{
	"/_matrix/widget/rest/v1/register",
	{
		"(undocumented) register"
	}
};

decltype(ircd::m::widget::register_post)
ircd::m::widget::register_post
{
	register_resource, "POST", handle_post,
	{
		//register_post.REQUIRES_AUTH
	}
};

ircd::m::resource::response
ircd::m::widget::handle_post(client &client,
                             const resource::request &request)
{
	const string_view version
	{
		request.query["v"]
	};

	const json::string token_type
	{
		request["token_type"]
	};

	const json::string matrix_server_name
	{
		request["matrix_server_name"]
	};

	const json::string &access_token
	{
		request["access_token"]
	};

	const seconds expires_in
	{
		request.get("expires_in", seconds(0))
	};

	const bool can_verify
	{
		iequals(token_type, "Bearer")
		&& my_host(matrix_server_name)
	};

	if(!can_verify)
		throw m::error
		{
			http::UNAUTHORIZED, "M_MISSING_TOKEN",
			"Credentials for this method are required but missing."
		};

	const m::room::id::buf tokens
	{
		"tokens", matrix_server_name
	};

	const event::idx event_idx
	{
		m::room(tokens).get(std::nothrow, "ircd.access_token", access_token)
	};

	if(!event_idx)
		throw m::error
		{
			http::UNAUTHORIZED, "M_UNKNOWN_TOKEN",
			"Credentials for this method are required but invalid."
		};

	m::user::id::buf user_id_buf;
	const string_view user_id
	{
		m::get(std::nothrow, event_idx, "sender", user_id_buf)
	};

	return resource::response
	{
		client, http::OK, json::members
		{
			{ "scalar_token", access_token }
		}
	};
}
