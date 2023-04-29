// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include "rooms.h"

using namespace ircd;

static void
relations_chunk_append(client &client,
                       const m::resource::request &request,
                       const m::event::idx &event_idx,
                       m::event::fetch &event,
                       json::stack::array &chunk);

static void
relations_chunk(client &client,
                const m::resource::request &request,
                const m::room::id &room_id,
                const m::event::id &event_id,
                const m::event::idx &event_idx,
                const string_view &rel_type,
                json::stack::array &chunk);

m::resource::response
get__relations(client &client,
               const m::resource::request &request,
               const m::room::id &room_id)
{
	if(!m::exists(room_id))
		throw m::NOT_FOUND
		{
			"Cannot find relations in %s which is not found.",
			string_view{room_id}
		};

	if(request.parv.size() < 3)
		throw m::NEED_MORE_PARAMS
		{
			"event_id path parameter required"
		};

	m::event::id::buf event_id
	{
		url::decode(event_id, request.parv[2])
	};

	// Get the rel_type path parameter.
	// Note: Not requiring path parameter; empty will mean all types.
	char rel_type_buf[m::event::TYPE_MAX_SIZE];
	if((false) && request.parv.size() < 4)
		throw m::NEED_MORE_PARAMS
		{
			"relation rel_type path parameter required"
		};

	const string_view rel_type
	{
		request.parv.size() > 3?
			url::decode(rel_type_buf, request.parv[3]):
			string_view{}
	};

	// Get the alleged type path parameter.
	// Note: Not requiring this path parameter either; it is not clear yet
	// what this parameter actually means.
	char type_buf[m::event::TYPE_MAX_SIZE];
	if((false) && request.parv.size() < 5)
		throw m::NEED_MORE_PARAMS
		{
			"relation ?type? path parameter required"
		};

	const string_view type
	{
		request.parv.size() > 4?
			url::decode(type_buf, request.parv[4]):
			string_view{}
	};

	const auto event_idx
	{
		m::index(event_id)
	};

	m::resource::response::chunked response
	{
		client, http::OK
	};

	json::stack out
	{
		response.buf, response.flusher()
	};

	json::stack::object top
	{
		out
	};

	json::stack::array chunk
	{
		top, "chunk"
	};

	relations_chunk
	(
		client, request, room_id, event_id, event_idx, rel_type, chunk
	);

	return response;
}

void
relations_chunk(client &client,
                const m::resource::request &request,
                const m::room::id &room_id,
                const m::event::id &event_id,
                const m::event::idx &event_idx,
                const string_view &rel_type,
                json::stack::array &chunk)
try
{
	const m::relates relates
	{
		// Find relations to this event
		.refs = event_idx,

		// Relations may require the same sender as event_idx.
		.match_sender = rel_type == "m.replace"
	};

	m::event::fetch event;
	relates.rfor_each(rel_type, [&client, &request, &event, &chunk]
	(const m::event::idx &event_idx, const json::object &, const m::relates_to &)
	{
		relations_chunk_append(client, request, event_idx, event, chunk);
	});
}
catch(const std::exception &e)
{
	log::error
	{
		m::log, "relations in %s for %s rel_type:%s by %s :%s",
		string_view{room_id},
		string_view{event_id},
		rel_type,
		string_view{request.user_id},
		e.what(),
	};
}

void
relations_chunk_append(client &client,
                       const m::resource::request &request,
                       const m::event::idx &event_idx,
                       m::event::fetch &event,
                       json::stack::array &chunk)

{
	if(unlikely(!seek(std::nothrow, event, event_idx)))
		return;

	if(!visible(event, request.user_id))
		return;

	m::event::append
	{
		chunk, event,
		{
			.event_idx = event_idx,
			.user_id = request.user_id,
			.query_txnid = false,
		}
	};
}
