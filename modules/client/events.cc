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
	"Client 6.2.2 :Events"
};

resource
events_resource
{
	"/_matrix/client/r0/events",
	{
		"Events (6.2.3) (10.x)"
	}
};

resource::response
get__events(client &client, const resource::request &request)
{
	const m::room::id &room_id
	{
		unquote(request["room_id"])
	};

	const m::vm::query<m::vm::where::equal> query
	{
		{ "room_id", room_id }
	};

	size_t i(0);
	m::vm::for_each(query, [&i](const auto &event)
	{
		++i;
	});

	size_t j(0);
	json::value ret[i];
	m::vm::for_each(query, [&i, &j, &ret](const m::event &event)
	{
		if(j < i)
			ret[j++] = event;
	});

	return resource::response
	{
		client, json::members
		{
			{ "chunk", { ret, j } }
		}
	};
}

resource::method
method_get
{
	events_resource, "GET", get__events
};
