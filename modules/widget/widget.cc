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
	extern resource::method widget_get;
	extern resource widget_resource;
}

ircd::mapi::header
IRCD_MODULE
{
	"Widget 1.0 :Widget",
};

decltype(ircd::m::widget::widget_resource)
ircd::m::widget::widget_resource
{
	"/_matrix/widget/",
	{
		"(undocumented) widget"
	}
};

decltype(ircd::m::widget::widget_get)
ircd::m::widget::widget_get
{
	widget_resource, "GET", handle_get
};

ircd::m::resource::response
ircd::m::widget::handle_get(client &client,
                            const resource::request &request)
{
	char widget_id_buf[256];
	const string_view widget_id
	{
		url::decode(widget_id_buf, request.query["widgetId"])
	};

	char parent_url_buf[384];
	const string_view parent_url
	{
		url::decode(parent_url_buf, request.query["parentUrl"])
	};

	return resource::response
	{
		client, http::OK
	};
}
