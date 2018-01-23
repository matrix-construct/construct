// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

using namespace ircd;

mapi::header IRCD_MODULE
{
	"federation missing events handler"
};

struct send
:resource
{
	using resource::resource;
}
get_missing_events_resource
{
	"/_matrix/federation/v1/get_missing_events/",
	{
		"federation missing events handler",
		resource::DIRECTORY,
	}
};

resource::response
handle_get(client &client,
           const resource::request &request)
{
	m::room::id::buf room_id
	{
		url::decode(request.parv[0], room_id)
	};

	return resource::response
	{
		client, http::NOT_FOUND
	};
}

resource::method method_get
{
	get_missing_events_resource, "GET", handle_get,
	{
		method_get.VERIFY_ORIGIN
	}
};
