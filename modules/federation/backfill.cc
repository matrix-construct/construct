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

conf::item<size_t>
backfill_limit_max
{
	{ "name",     "ircd.federation.backfill.limit.max" },
	{ "default",  512L /* TODO: hiiigherrrr */         },
};

conf::item<size_t>
backfill_limit_default
{
	{ "name",     "ircd.federation.backfill.limit.default" },
	{ "default",  64L                                      },
};

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

resource::response
get__backfill(client &client,
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

	const size_t limit
	{
		calc_limit(request)
	};

	m::room::messages it
	{
		room_id, event_id
	};

	 //TODO: chunk direct to socket
	const unique_buffer<mutable_buffer> buf
	{
		64_KiB * limit //TODO: XXX
	};

	json::stack out{buf};
	{
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
			const m::event &event{*it};
			pdus.append(event);
		}
	}

	return resource::response
	{
		client, json::object
		{
			out.completed()
		}
	};
}

resource::method
method_get
{
	backfill_resource, "GET", get__backfill,
	{
		method_get.VERIFY_ORIGIN
	}
};
