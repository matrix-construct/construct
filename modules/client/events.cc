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
	"Client 11.16 :Room Previews"
};

resource
events_resource
{
	"/_matrix/client/r0/events",
	{
		"(11.16) Room Previews"
	}
};

resource::response
get__events(client &client,
            const resource::request &request)
{
	if(!request.query["room_id"])
		throw m::UNSUPPORTED
		{
			"Specify a room_id or use /sync"
		};

	m::room::id::buf room_id
	{
		url::decode(request.query["room_id"], room_id)
	};

	const auto &from
	{
		request.query["from"]
	};

	const auto &requested_timeout
	{
		//TODO: conf
		request.query["timeout"]? lex_cast<uint>(request.query["timeout"]) : 30000U
	};

	const milliseconds timeout
	{
		//TODO: conf
		std::min(requested_timeout, 60000U)
	};

	const m::room room
	{
		room_id
	};

	//TODO: X
	ctx::sleep(timeout);

	const string_view start{};
	const string_view end{};
	std::vector<json::value> events;
	json::value chunk
	{
		events.data(), events.size()
	};

	return resource::response
	{
		client, json::members
		{
			{ "start",  start },
			{ "chunk",  chunk },
			{ "end",    end   },
		}
	};
}

resource::method
method_get
{
	events_resource, "GET", get__events
};
