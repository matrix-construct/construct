// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2023 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

using namespace ircd;

mapi::header
IRCD_MODULE
{
	"federation timestamp to event"
};

m::resource
timestamp_to_event_resource
{
	"/_matrix/federation/v1/timestamp_to_event/",
	{
		"federation timestamp to event",
		resource::DIRECTORY,
	}
};

m::resource::response
get__timestamp_to_event(client &client,
                        const m::resource::request &request);

m::resource::method
method_get
{
	timestamp_to_event_resource, "GET", get__timestamp_to_event,
	{
		method_get.VERIFY_ORIGIN
	}
};

m::resource::response
get__timestamp_to_event(client &client,
                        const m::resource::request &request)
{
	if(request.parv.size() < 1)
		throw m::NEED_MORE_PARAMS
		{
			"room_id path parameter required"
		};

	m::room::id::buf room_id
	{
		url::decode(room_id, request.parv[0])
	};

	if(m::room::server_acl::enable_read && !m::room::server_acl::check(room_id, request.node_id))
		throw m::ACCESS_DENIED
		{
			"You are not permitted by the room's server access control list."
		};

	const auto dir
	{
		request.query["dir"]
	};

	const milliseconds ts
	{
		request.query.at<long>("ts")
	};

	const m::event::idx event_idx
	{
		0UL
	};

	const m::event::fetch event
	{
		std::nothrow, event_idx
	};

	const long &event_ts
	{
		json::get<"origin_server_ts"_>(event)
	};

	return m::resource::response
	{
		client, http::NOT_IMPLEMENTED, json::members
		{
			{ "event_id", event.event_id },
			{ "origin_server_ts", event_ts },
		}
	};
}
