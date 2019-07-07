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

resource::response
get__members(client &client,
             const resource::request &request,
             const m::room::id &room_id)
{
	char membershipbuf[32];
	const auto &membership
	{
		url::decode(membershipbuf, request.query["membership"])
	};

	char not_membershipbuf[32];
	const auto &not_membership
	{
		url::decode(not_membershipbuf, request.query["not_membership"])
	};

	char atbuf[48];
	const string_view at
	{
		url::decode(atbuf, request.query["at"])
	};

	const auto event_idx
	{
		// at is a /sync since token we gave the client. This is simply
		// an event_idx sequence integer, except during phased-polylog sync
		// when this is a negative integer. If this is phased sync, we can
		// parse this token for the snapshot integer.
		at && !startswith(at, '-')?
			lex_cast<m::event::idx>(at):
		startswith(at, '-')?
			lex_cast<m::event::idx>(split(at, '_').second):
			m::event::idx{0}
	};

	const m::event::id::buf event_id
	{
		event_idx && event_idx <= m::vm::sequence::retired?
			m::event_id(event_idx, std::nothrow):
			m::event::id::buf{}
	};

	const m::room room
	{
		room_id, event_id
	};

	if(!event_id && !exists(room))
		throw m::NOT_FOUND
		{
			"Room %s does not exist.",
			string_view{room_id}
		};

	if(!room.visible(request.user_id))
		throw m::ACCESS_DENIED
		{
			"You do not have permission to view %s members.",
			string_view{room_id}
		};

	resource::response::chunked response
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

	const m::room::members members
	{
		room
	};

	members.for_each(membership, m::event::closure_bool{[&request, &chunk, &not_membership]
	(const m::event &event)
	{
		if(m::membership(event) == not_membership)
			return true;

		chunk.append(event);
		return true;
	}});

	return std::move(response);
}

resource::response
get__joined_members(client &client,
                    const resource::request &request,
                    const m::room::id &room_id)
{
	const m::room room
	{
		room_id
	};

	if(!exists(room))
		throw m::NOT_FOUND
		{
			"Room %s does not exist.",
			string_view{room_id}
		};

	if(!room.visible(request.user_id))
		throw m::ACCESS_DENIED
		{
			"You do not have permission to view %s joined members.",
			string_view{room_id}
		};

	resource::response::chunked response
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

	json::stack::object joined
	{
		top, "joined"
	};

	const m::room::members members
	{
		room
	};

	members.for_each("join", m::room::members::closure{[&joined, &room]
	(const m::user::id &user_id)
	{
		const m::event::idx &event_idx
		{
			room.get(std::nothrow, "m.room.member", user_id)
		};

		if(!event_idx)
			return;

		json::stack::object room_member
		{
			joined, user_id
		};

		m::get(std::nothrow, event_idx, "content", [&room_member]
		(const json::object &content)
		{
			for(const auto &[key, val] : content)
				json::stack::member
				{
					room_member, key, val
				};
		});
	}});

	return std::move(response);
}
