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

mapi::header
IRCD_MODULE
{
	"federation event"
};

m::resource
event_resource
{
	"/_matrix/federation/v1/event/",
	{
		"federation event",
		resource::DIRECTORY,
	}
};

static m::resource::response
handle_get(client &,
           const m::resource::request &);

m::resource::method
method_get
{
	event_resource, "GET", handle_get,
	{
		method_get.VERIFY_ORIGIN
	}
};

m::resource::response
handle_get(client &client,
           const m::resource::request &request)
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

	const m::event::fetch event
	{
		event_id
	};

	if(!visible(event, request.node_id))
		throw m::ACCESS_DENIED
		{
			"You are not permitted to view this event"
		};

	m::resource::response::chunked::json response
	{
		client, http::OK
	};

	json::stack::member
	{
		response, "origin", json::value
		{
			origin(m::my())
		}
	};

	json::stack::member
	{
		response, "origin_server_ts", json::value
		{
			time<milliseconds>()
		}
	};

	json::stack::array pdus
	{
		response, "pdus"
	};

	pdus.append(event);
	return response;
}
