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

resource::response
get__relations(client &client,
               const resource::request &request,
               const m::room::id &room_id)
{
	if(!m::exists(room_id))
		throw m::NOT_FOUND
		{
			"Cannot take a report about %s which is not found.",
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

	if(request.parv.size() < 4)
		throw m::NEED_MORE_PARAMS
		{
			"relation first path parameter required"
		};

	if(request.parv.size() < 5)
		throw m::NEED_MORE_PARAMS
		{
			"relation second path parameter required"
		};

	char rbuf[2][256];
	const string_view r[2]
	{
		url::decode(rbuf[0], request.parv[3]),
		url::decode(rbuf[1], request.parv[4]),
	};

	const unique_buffer<mutable_buffer> buf
	{
		96_KiB
	};

	json::stack out{buf};
	json::stack::object top
	{
		out
	};

	m::event::idx next_idx
	{
		index(event_id, std::nothrow)
	};

	json::stack::array chunk
	{
		top, "chunk"
	};

	const m::event::fetch event
	{
		next_idx, std::nothrow
	};

	if(event.valid)
		chunk.append(event);

	const auto each_ref{[&r, &next_idx, &chunk]
	(const m::event::idx &event_idx, const m::dbs::ref &)
	{
		const m::event::fetch event
		{
			event_idx, std::nothrow
		};

		if(!event.valid)
			return true;

		const json::object &m_relates_to
		{
			json::get<"content"_>(event).get("m.relates_to")
		};

		const json::string &rel_type
		{
			m_relates_to.get("rel_type")
		};

		if(rel_type != r[0])
			return true;

		chunk.append(event);
		next_idx = event_idx;
		return false;
	}};

	while(next_idx) try
	{
		const m::event::refs refs
		{
			next_idx
		};

		if(refs.for_each(m::dbs::ref::M_RELATES, each_ref))
			break;
	}
	catch(const std::exception &e)
	{
		log::error
		{
			m::log, "relation trace from %s on %ld :%s",
			string_view{event_id},
			next_idx,
			e.what()
		};

		break;
	}

	chunk.~array();
	top.~object();

	return resource::response
	{
		client, json::object
		{
			out.completed()
		}
	};
}
