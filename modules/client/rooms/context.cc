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

const size_t
default_limit
{
	// 11.20.1.1 - The maximum number of events to return. Default: 10.
	10
};

conf::item<size_t>
limit_max
{
	{ "name",     "ircd.client.rooms.context.limit.max" },
	{ "default",  128L                                  },
};

conf::item<size_t>
flush_hiwat
{
	{ "name",     "ircd.client.rooms.context.flush.hiwat" },
	{ "default",  16384L                                  },
};

const m::event::fetch::opts
default_fetch_opts
{
	m::event::keys::include
	{
		"content",
		"depth",
		"event_id",
		"membership",
		"origin_server_ts",
		"redacts",
		"room_id",
		"sender",
		"state_key",
		"type",
	},
};

resource::response
get__context(client &client,
             const resource::request &request,
             const m::room::id &room_id)
{
	m::event::id::buf event_id
	{
		url::decode(request.parv[2], event_id)
	};

	const auto limit{[&request]
	{
		auto ret(request.query.get<size_t>("limit", default_limit));
		return std::min(ret, size_t(limit_max));
	}()};

	const m::room room
	{
		room_id, event_id
	};

	if(!room.visible(request.user_id))
		throw m::ACCESS_DENIED
		{
			"You are not permitted to view the room at this event"
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

	const auto flush{[&response]
	(const const_buffer &buf)
	{
		response.write(buf);
		return buf;
	}};

	json::stack out
	{
		buf, flush, size_t(flush_hiwat)
	};

	json::stack::object ret
	{
		out
	};

	json::stack::member
	{
		ret, "event", event
	};

	m::event::id::buf start{event_id};
	{
		json::stack::member member{ret, "events_before"};
		json::stack::array array{member};
		m::room::messages before
		{
			room, event_id, &default_fetch_opts
		};

		if(before)
			--before;

		for(size_t i(0); i < limit && before; --before, ++i)
		{
			const m::event &event{*before};
			if(!visible(event, request.user_id))
				break;

			start = at<"event_id"_>(event);
			array.append(event);
		}
	}

	json::stack::member
	{
		ret, "start", json::value{start}
	};

	m::event::id::buf end{event_id};
	{
		json::stack::member member{ret, "events_after"};
		json::stack::array array{member};
		m::room::messages after
		{
			room, event_id, &default_fetch_opts
		};

		if(after)
			++after;

		for(size_t i(0); i < limit && after; ++after, ++i)
		{
			const m::event &event{*after};
			if(!visible(event, request.user_id))
				break;

			end = at<"event_id"_>(event);
			array.append(event);
		}
	}

	json::stack::member
	{
		ret, "end", json::value{end}
	};

	{
		json::stack::member member{ret, "state"};
		json::stack::array array{member};
		const m::room::state state
		{
			room, &default_fetch_opts
		};

		state.for_each([&array, &request]
		(const m::event &event)
		{
			if(!visible(event, request.user_id))
				return;

			array.append(event);
		});
	}

	return {};
}
