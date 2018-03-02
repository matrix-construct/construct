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

	const unique_buffer<mutable_buffer> buffer
	{
		64_KiB
	};

	const m::event::fetch event
	{
		event_id
	};

	const json::value pdu
	{
		static_cast<const m::event &>(event)
	};

	return resource::response
	{
		client, json::members
		{
			{ "pdus", { &pdu, 1 } }
		}
	};
}

resource::method method_get
{
	event_resource, "GET", handle_get,
	{
		method_get.VERIFY_ORIGIN
	}
};
