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

mapi::header
IRCD_MODULE
{
	"Client 14.13.1.3 :Listing Notifications"
};

ircd::resource
notifications_resource
{
	"/_matrix/client/r0/notifications",
	{
		"(14.13.1.3) Listing Notifications"
	}
};

conf::item<size_t>
limit_default
{
	{ "name",    "ircd.client.notifications.limit.default" },
	{ "default",  256                                      }
};

resource::response
get__notifications(client &client,
                   const resource::request &request)
{
	const string_view &from
	{
		request.query["from"]
	};

	const string_view &only
	{
		request.query["only"]
	};

	const ushort limit
	{
		request.query.get<ushort>("limit", size_t(limit_default))
	};

	return resource::response
	{
		client, http::OK
	};
}

resource::method
method_get
{
	notifications_resource, "GET", get__notifications,
	{
		method_get.REQUIRES_AUTH
	}
};
