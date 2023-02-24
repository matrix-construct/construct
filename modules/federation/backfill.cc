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

m::resource
backfill_resource
{
	"/_matrix/federation/v1/backfill/",
	{
		"federation backfill",
		resource::DIRECTORY,
	}
};

static size_t
calc_limit(const m::resource::request &request);

m::resource::response
get__backfill(client &client,
              const m::resource::request &request);

m::resource::method
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

m::resource::response
get__backfill(client &client,
              const m::resource::request &request)
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

	if(m::room::server_acl::enable_read && !m::room::server_acl::check(room_id, request.node_id))
		throw m::ACCESS_DENIED
		{
			"You are not permitted by the room's server access control list."
		};

	m::event::id::buf event_id
	{
		request.query["v"]?
			url::decode(event_id, request.query.at("v")):
			m::head(room_id)
	};

	const bool ids_only
	{
		request.query.get<bool>("pdu_ids", false)
	};

	const size_t limit
	{
		calc_limit(request)
	};

	m::room::events it
	{
		room_id, event_id
	};

	m::resource::response::chunked response
	{
		client, http::OK
	};

	json::stack out
	{
		response.buf, response.flusher(), size_t(backfill_flush_hiwat)
	};

	json::stack::object top
	{
		out
	};

	json::stack::member
	{
		top, "origin", json::value
		{
			origin(m::my())
		}
	};

	json::stack::member
	{
		top, "origin_server_ts", json::value
		{
			time<milliseconds>()
		}
	};

	json::stack::array pdus
	{
		top, ids_only? "pdu_ids": "pdus"
	};

	size_t count{0};
	m::event::fetch event;
	for(; it && count < limit; ++count, --it)
	{
		if(!seek(std::nothrow, event, it.event_idx()))
			continue;

		assert(event.event_id);
		if(!visible(event, request.node_id))
			continue;

		if(ids_only)
			pdus.append(event.event_id);
		else
			pdus.append(event);
	}

	return response;
}

static size_t
calc_limit(const m::resource::request &request)
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
