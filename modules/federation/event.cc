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
	m::event::id::buf event_id
	{
		url::decode(request.parv[0], event_id)
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

	const unique_buffer<mutable_buffer> buf
	{
		96_KiB
	};

	resource::response::chunked response
	{
		client, http::OK
	};

	json::stack out
	{
		buf, [&response](const const_buffer &buf)
		{
			response.write(buf);
			return buf;
		}
	};

	json::stack::object top{out};
	json::stack::member pdus_m
	{
		top, "pdus"
	};

	json::stack::array pdus
	{
		pdus_m
	};

	pdus.append(event);
	return {};
}

resource::method method_get
{
	event_resource, "GET", handle_get,
	{
		method_get.VERIFY_ORIGIN
	}
};
