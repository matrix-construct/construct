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
	extern resource::method ui_get;
	extern resource ui_resource;
}

ircd::mapi::header
IRCD_MODULE
{
	"Widget 1.0 :UI",
};

decltype(ircd::m::widget::ui_resource)
ircd::m::widget::ui_resource
{
	"/_matrix/widget/ui/v1",
	{
		"(undocumented) UI v1"
	}
};

decltype(ircd::m::widget::ui_get)
ircd::m::widget::ui_get
{
	ui_resource, "GET", handle_get,
	{
		//ui_get.REQUIRES_AUTH
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

	m::room::id::buf room_id
	{
		url::decode(room_id, request.query["room_id"])
	};

	char room_name_buf[256];
	const string_view room_name
	{
		url::decode(room_name_buf, request.query["room_name"])
	};

	char theme_buf[64];
	const string_view theme
	{
		url::decode(theme_buf, request.query["theme"])
	};

	char integ_id_buf[48];
	const string_view integ_id
	{
		url::decode(integ_id_buf, request.query["integ_id"])
	};

	char screen_buf[48];
	const string_view screen
	{
		url::decode(screen_buf, request.query["screen"])
	};

	return resource::response
	{
		client, http::OK
	};
}
