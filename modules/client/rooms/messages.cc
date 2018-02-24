// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include "rooms.h"

using namespace ircd;

struct pagination_tokens
{
	uint8_t limit;
	char dir;
	m::event::id::buf from;
	m::event::id::buf to;

	pagination_tokens(const resource::request &);
};

resource::response
get__messages(client &client,
              const resource::request &request,
              const m::room::id &room_id)
{
	const pagination_tokens page
	{
		request
	};

	const string_view &filter
	{
		request.query["filter"]
	};

	const m::room room
	{
		room_id
	};

	m::event::id::buf start, end;
	std::vector<json::value> ret;
	ret.reserve(page.limit);

	m::room::messages it
	{
		room, page.from
	};

	// Spec sez the 'from' token is exclusive
	if(it && page.dir == 'b')
		--it;
	else if(it)
		++it;

	for(; it; page.dir == 'b'? --it : ++it)
	{
		const m::event &event{*it};
		if(page.to && at<"event_id"_>(event) == page.to)
		{
			if(page.dir != 'b')
				start = at<"event_id"_>(event);

			break;
		}

		ret.emplace_back(event);
		if(ret.size() >= page.limit)
		{
			if(page.dir == 'b')
				end = at<"event_id"_>(event);
			else
				start = at<"event_id"_>(event);

			break;
		}
	}

	const json::value chunk
	{
		ret.data(), ret.size()
	};

	return resource::response
	{
		client, json::members
		{
			{ "start",  start },
			{ "end",    end   },
			{ "chunk",  chunk },
		}
	};
}

// Client-Server 6.3.6 query parameters
pagination_tokens::pagination_tokens(const resource::request &request)
try
:limit
{
	// The maximum number of events to return. Default: 10.
	// > we limit this to 255 via narrowing cast
	request.query["limit"]?
		uint8_t(lex_cast<ushort>(request.query.at("limit"))):
		uint8_t(10)
}
,dir
{
	// Required. The direction to return events from. One of: ["b", "f"]
	request.query.at("dir").at(0)
}
,from
{
	// Required. The token to start returning events from. This token can be
	// obtained from a prev_batch token returned for each room by the sync
	// API, or from a start or end token returned by a previous request to
	// this endpoint.
	url::decode(request.query.at("from"), from)
}
{
	// The token to stop returning events at. This token can be obtained from
	// a prev_batch token returned for each room by the sync endpoint, or from
	// a start or end token returned by a previous request to this endpoint.
	if(!empty(request.query["to"]))
		url::decode(request.query.at("to"), to);

	if(dir != 'b' && dir != 'f')
		throw m::BAD_PAGINATION
		{
			"query parameter 'dir' must be 'b' or 'f'"
		};
}
catch(const bad_lex_cast &)
{
	throw m::BAD_PAGINATION
	{
		"query parameter 'limit' is invalid"
	};
}
catch(const m::INVALID_MXID &)
{
	throw m::BAD_PAGINATION
	{
		"query parameter 'from' or 'to' is not a valid token"
	};
}
