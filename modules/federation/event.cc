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
	"federation event"
};

struct send
:resource
{
	using resource::resource;
}
event_resource
{
	"/_matrix/federation/v1/event/",
	{
		"federation event",
		resource::DIRECTORY,
	}
};

resource::response
handle_get(client &client,
           const resource::request &request)
{
	if(request.parv.size() < 1)
		throw m::NEED_MORE_PARAMS
		{
			"event_id path parameter required."
		};

	m::event::id::buf event_id
	{
		url::decode(event_id, request.parv[0])
	};

	if(!visible(event_id, request.node_id))
		throw m::ACCESS_DENIED
		{
			"You are not permitted to view this event"
		};

	const m::event::fetch event
	{
		event_id
	};

	resource::response::chunked response
	{
		client, http::OK
	};

	json::stack out
	{
		response.buf, response.flusher()
	};

	json::stack::object top{out};
	json::stack::array pdus
	{
		top, "pdus"
	};

	pdus.append(event);
	return std::move(response);
}

resource::method method_get
{
	event_resource, "GET", handle_get,
	{
		method_get.VERIFY_ORIGIN
	}
};
