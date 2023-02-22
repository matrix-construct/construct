// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2023 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include "rooms.h"

using namespace ircd;

static int64_t
threads_chunk(json::stack::array &chunk,
              const m::resource::request &request,
              const m::room::id &room_id,
              const int64_t &from,
              const string_view &include,
              const size_t &limit);

static conf::item<size_t>
threads_limit_default
{
	{ "name",    "ircd.client.rooms.threads.limit.default" },
	{ "default",  32                                       },
};

static conf::item<size_t>
threads_limit_max
{
	{ "name",    "ircd.client.rooms.threads.limit.max" },
	{ "default",  256                                  },
};

m::resource::response
get__threads(client &client,
             const m::resource::request &request,
             const m::room::id &room_id)
{
	if(!m::exists(room_id))
		throw m::NOT_FOUND
		{
			"Cannot find threads for %s which is not found.",
			string_view{room_id}
		};

	if(!m::visible(room_id, request.user_id))
		throw m::FORBIDDEN
		{
			"You are not allowed to view threads of %s",
			string_view{room_id},
		};

	const auto include
	{
		request.query.get("include", "all"_sv)
	};

	const auto from
	{
		request.query.get<int64_t>("from", m::depth(room_id))
	};

	const auto limit
	{
		std::clamp
		(
			request.query.get("limit", size_t(threads_limit_default)),
			1UL,
			size_t(threads_limit_max)
		)
	};

	m::resource::response::chunked::json response
	{
		client, http::OK
	};

	int64_t next_batch {0};
	{
		json::stack::array chunk
		{
			response, "chunk"
		};

		next_batch = threads_chunk(chunk, request, room_id, from, include, limit);
	}

	if(next_batch)
		json::stack::member
		{
			response, "next_batch", json::value
			{
				next_batch
			}
		};

	return response;
}

int64_t
threads_chunk(json::stack::array &chunk,
              const m::resource::request &request,
              const m::room::id &room_id,
              const int64_t &from,
              const string_view &include,
              const size_t &limit)
{
	return 0;
}
