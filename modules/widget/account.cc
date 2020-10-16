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
	static resource::response handle_get(client &, const resource::request &);
	extern resource::method account_get;
	extern resource account_resource;
}

ircd::mapi::header
IRCD_MODULE
{
	"Widget 1.0 :Account",
};

decltype(ircd::m::widget::account_resource)
ircd::m::widget::account_resource
{
	"/_matrix/widget/rest/v1/account",
	{
		"(undocumented) account"
	}
};

decltype(ircd::m::widget::account_get)
ircd::m::widget::account_get
{
	account_resource, "GET", handle_get,
	{
		//account_get.REQUIRES_AUTH
	}
};

ircd::m::resource::response
ircd::m::widget::handle_get(client &client,
                            const resource::request &request)
{
	const string_view version
	{
		request.query["v"]
	};

	const string_view scalar_token
	{
		request.query["scalar_token"]
	};

	if(!scalar_token)
		throw m::error
		{
			http::UNAUTHORIZED, "M_MISSING_TOKEN",
			"Credentials for this method are required but missing."
		};

	const m::room::id::buf tokens
	{
		"tokens", origin(my())
	};

	const event::idx event_idx
	{
		m::room(tokens).get(std::nothrow, "ircd.access_token", scalar_token)
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
		client, http::OK
	};
}
