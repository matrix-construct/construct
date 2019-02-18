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
	"federation backfill"
};

resource
backfill_resource
{
	"/_matrix/federation/v1/backfill/",
	{
		"federation backfill",
		resource::DIRECTORY,
	}
};

static size_t
calc_limit(const resource::request &request);

resource::response
get__backfill(client &client,
              const resource::request &request);

resource::method
method_get
{
	backfill_resource, "GET", get__backfill,
	{
		method_get.VERIFY_ORIGIN
	}
};

conf::item<size_t>
backfill_limit_max
{
	{ "name",     "ircd.federation.backfill.limit.max" },
	{ "default",  16384L                               },
};

conf::item<size_t>
backfill_limit_default
{
	{ "name",     "ircd.federation.backfill.limit.default" },
	{ "default",  64L                                      },
};

conf::item<size_t>
backfill_flush_hiwat
{
	{ "name",     "ircd.federation.backfill.flush.hiwat" },
	{ "default",  16384L                                 },
};

resource::response
get__backfill(client &client,
              const resource::request &request)
{
	if(request.parv.size() < 1)
		throw m::NEED_MORE_PARAMS
		{
			"room_id path parameter required"
		};

	m::room::id::buf room_id
	{
		url::decode(room_id, request.parv[0])
	};

	m::event::id::buf event_id
	{
		request.query["v"]?
			url::decode(event_id, request.query.at("v")):
			m::head(room_id)
	};

	if(!visible(event_id, request.node_id))
		throw m::ACCESS_DENIED
		{
			"You are not permitted to view the room at this event."
		};

	const size_t limit
	{
		calc_limit(request)
	};

	m::room::messages it
	{
		room_id, event_id
	};

	resource::response::chunked response
	{
		client, http::OK
	};

	json::stack out
	{
		response.buf, response.flusher(), size_t(backfill_flush_hiwat)
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

	size_t count{0};
	for(; it && count < limit; ++count, --it)
	{
		const m::event &event(*it);
		if(!visible(event, request.node_id))
			continue;

		pdus.append(event);
	}

	return {};
}

static size_t
calc_limit(const resource::request &request)
{
	const auto &limit
	{
		request.query["limit"]
	};

	if(!limit)
		return size_t(backfill_limit_default);

	size_t ret
	{
		lex_cast<size_t>(limit)
	};

	return std::min(ret, size_t(backfill_limit_max));
}
