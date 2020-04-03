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
relations_chunk(client &client,
                const m::resource::request &request,
                const m::room::id &room_id,
                const m::event::id &event_id,
                const string_view &rel_type,
                const string_view &type,
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

	if(!m::exists(event_id))
		throw m::NOT_FOUND
		{
			"Cannot get relations about %s which is not found.",
			string_view{event_id}
		};

	// Get the rel_type path parameter.
	// Note: Not requiring path parameter; empty will mean all types.
	char rel_type_buf[m::event::TYPE_MAX_SIZE];
	if((false) && request.parv.size() < 4)
		throw m::NEED_MORE_PARAMS
		{
			"relation rel_type path parameter required"
		};

	const string_view &rel_type
	{
		url::decode(rel_type_buf, request.parv[3])
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

	const string_view &type
	{
		url::decode(type_buf, request.parv[4])
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

	relations_chunk(client, request, room_id, event_id, rel_type, type, chunk);
	return std::move(response);
}

void
relations_chunk(client &client,
                const m::resource::request &request,
                const m::room::id &room_id,
                const m::event::id &event_id,
                const string_view &rel_type,
                const string_view &type,
                json::stack::array &chunk)
try
{
	const auto append{[&chunk, &request]
	(const auto &event_idx, const m::event &event)
	{
		m::event::append::opts opts;
		opts.event_idx = &event_idx;
		opts.user_id = &request.user_id;
		opts.query_txnid = false;
		m::event::append
		{
			chunk, event, opts
		};
	}};

	m::event::idx event_idx
	{
		index(std::nothrow, event_id)
	};

	m::event::fetch event
	{
		std::nothrow, event_idx
	};

	const auto each_ref{[&event, &append, &rel_type, &request]
	(const m::event::idx &event_idx, const m::dbs::ref &)
	{
		if(!seek(std::nothrow, event, event_idx))
			return true;

		const json::object &m_relates_to
		{
			at<"content"_>(event).get("m.relates_to")
		};

		const json::string &_rel_type
		{
			m_relates_to.get("rel_type")
		};

		if(_rel_type != rel_type)
			return true;

		if(!visible(event, request.user_id))
			return true;

		append(event_idx, event);
		return true;
	}};

	if(!event.valid)
		return;

	if(!visible(event, request.user_id))
		return;

	// Send the original event
	append(event_idx, event);

	// Send all the referencees
	const m::event::refs refs{event_idx};
	refs.for_each(m::dbs::ref::M_RELATES, each_ref);
}
catch(const std::exception &e)
{
	log::error
	{
		m::log, "relations in %s for %s rel_type:%s type:%s by %s :%s",
		string_view{room_id},
		string_view{event_id},
		rel_type,
		type,
		string_view{request.user_id},
		e.what(),
	};
}
