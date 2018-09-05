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

conf::item<size_t>
max_filter_miss
{
	{ "name",      "ircd.client.rooms.messages.max_filter_miss" },
	{ "default",   2048L                                        },
};

static const m::event::fetch::opts
default_fetch_opts
{
	m::event::keys::include
	{
		"content",
		"depth",
		"event_id",
		"membership",
		"origin_server_ts",
		"prev_events",
		"redacts",
		"room_id",
		"sender",
		"state_key",
		"type",
	},
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

	const auto &filter_query
	{
		request.query["filter"]
	};

	const unique_buffer<mutable_buffer> filter_buf
	{
		size(filter_query)
	};

	const json::object &filter_json
	{
		url::decode(filter_query, filter_buf)
	};

	const m::room_event_filter filter
	{
		filter_json.has("filter_json")?
			json::object{filter_json.get("filter_json")}:
			filter_json
	};

	const m::room room
	{
		room_id, page.from
	};

	if(!room.visible(request.user_id))
		throw m::ACCESS_DENIED
		{
			"You are not permitted to view the room at this event"
		};

	m::room::messages it
	{
		room, page.from, &default_fetch_opts
	};

	resource::response::chunked response
	{
		client, http::OK
	};

	json::stack out
	{
		response.buf, response.flusher()
	};

	json::stack::object ret
	{
		out
	};

	// Spec sez the 'from' token is exclusive
	if(it && page.dir == 'b')
		--it;
	else if(it)
		++it;

	size_t hit{0}, miss{0};
	m::event::id::buf start, end;
	{
		json::stack::member chunk{ret, "chunk"};
		json::stack::array messages{chunk};
		for(; it; page.dir == 'b'? --it : ++it)
		{
			const m::event &event{*it};
			if(!visible(event, request.user_id))
				break;

			if(page.to && at<"event_id"_>(event) == page.to)
			{
				if(page.dir != 'b')
					start = at<"event_id"_>(event);

				break;
			}

			if(empty(filter_json) || match(filter, event))
			{
				messages.append(event);
				++hit;
			}
			else ++miss;

			if(hit >= page.limit || miss >= size_t(max_filter_miss))
			{
				if(page.dir == 'b')
					end = at<"event_id"_>(event);
				else
					start = at<"event_id"_>(event);

				break;
			}
		}
	}

	json::stack::member
	{
		ret, "start", json::value{start}
	};

	json::stack::member
	{
		ret, "end", json::value{end}
	};

	return {};
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
