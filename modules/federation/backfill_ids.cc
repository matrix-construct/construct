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
	"federation backfill event IDs"
};

resource
backfill_ids_resource
{
	"/_matrix/federation/v1/backfill_ids/",
	{
		"federation backfill ID's",
		resource::DIRECTORY,
	}
};

conf::item<size_t>
backfill_ids_limit_max
{
	{ "name",     "ircd.federation.backfill_ids.limit.max" },
	{ "default",  131072L                                  },
};

conf::item<size_t>
backfill_ids_limit_default
{
	{ "name",     "ircd.federation.backfill_ids.limit.default" },
	{ "default",  64L                                          },
};

static size_t
calc_limit(const resource::request &request)
{
	const auto &limit
	{
		request.query["limit"]
	};

	if(!limit)
		return size_t(backfill_ids_limit_default);

	const size_t &ret
	{
		lex_cast<size_t>(limit)
	};

	return std::min(ret, size_t(backfill_ids_limit_max));
}

resource::response
get__backfill_ids(client &client,
                  const resource::request &request)
{
	m::room::id::buf room_id
	{
		url::decode(request.parv[0], room_id)
	};

	m::event::id::buf event_id
	{
		request.query["v"]?
			url::decode(request.query.at("v"), event_id):
			m::head(room_id)
	};

	const m::room room
	{
		room_id, event_id
	};

	if(!room.visible(request.node_id))
		throw m::ACCESS_DENIED
		{
			"You are not permitted to view the room at this event"
		};

	const size_t limit
	{
		calc_limit(request)
	};

	m::room::messages it
	{
		room
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
	json::stack::member pdus_m
	{
		top, "pdu_ids"
	};

	json::stack::array pdus
	{
		pdus_m
	};

	size_t count{0};
	for(; it && count < limit; ++count, --it)
	{
		const auto &event_id(it.event_id());
		if(!visible(event_id, request.node_id))
			continue;

		pdus.append(event_id);
	}

	return {};
}

resource::method
method_get
{
	backfill_ids_resource, "GET", get__backfill_ids,
	{
		method_get.VERIFY_ORIGIN
	}
};
