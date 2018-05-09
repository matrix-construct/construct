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

extern "C" m::event::id::buf
commit__m_receipt_m_read(const m::room::id &,
                         const m::user::id &,
                         const m::event::id &,
                         const time_t &);

extern "C" bool
exists__m_receipt_m_read(const m::room::id &,
                         const m::user::id &,
                         const m::event::id &);

extern "C" bool
fresher__m_receipt_m_read(const m::room::id &,
                          const m::user::id &,
                          const m::event::id &);

resource::response
post__receipt(client &client,
              const resource::request &request,
              const m::room::id &room_id)
{
	if(request.parv.size() < 3)
		throw m::NEED_MORE_PARAMS
		{
			"receipt type required"
		};

	if(request.parv.size() < 4)
		throw m::NEED_MORE_PARAMS
		{
			"event_id required"
		};

	const string_view &receipt_type
	{
		request.parv[2]
	};

	m::event::id::buf event_id
	{
		url::decode(request.parv[3], event_id)
	};

	return resource::response
	{
		client, http::OK
	};
}

m::event::id::buf
commit__m_receipt_m_read(const m::room::id &room_id,
                         const m::user::id &user_id,
                         const m::event::id &event_id,
                         const time_t &ms)
{
	if(!fresher__m_receipt_m_read(room_id, user_id, event_id))
		return {};

	const m::user::room user_room
	{
		user_id
	};

	const auto evid
	{
		send(user_room, user_id, "ircd.read", room_id,
		{
			{ "event_id", event_id },
			{ "ts",       ms       }
		})
	};

	const json::value event_ids[]
	{
		{ event_id }
	};

	const json::members m_read
	{
		{ "data",
		{
			{ "ts", ms }
		}},
		{ "event_ids", { event_ids, 1 } },
	};

	json::iov event, content;
	const json::iov::push push[]
	{
		{ event,    { "type",     "m.receipt" } },
		{ event,    { "room_id",  room_id     } },
		{ content,  { room_id,
		{
			{ "m.read",
			{
				{ user_id, m_read }
			}}
		}}}
	};

	m::vm::copts opts;
	opts.hash = false;
	opts.sign = false;
	opts.event_id = false;
	opts.origin = true;
	opts.origin_server_ts = false;
	opts.conforming = false;
	return m::vm::eval
	{
		event, content, opts
	};
}

bool
fresher__m_receipt_m_read(const m::room::id &room_id,
                          const m::user::id &user_id,
                          const m::event::id &event_id)
try
{
	const m::user::room user_room
	{
		user_id
	};

	bool ret{true};
	user_room.get("ircd.read", room_id, [&ret, &event_id]
	(const m::event &event)
	{
		const auto &content
		{
			at<"content"_>(event)
		};

		const m::event::id &previous_id
		{
			unquote(content.get("event_id"))
		};

		if(event_id == previous_id)
		{
			ret = false;
			return;
		}

		const m::event::idx &previous_idx
		{
			index(previous_id)
		};

		const m::event::idx &event_idx
		{
			index(event_id)
		};

		ret = event_idx > previous_idx;
	});

	return ret;
}
catch(const std::exception &e)
{
	log::derror
	{
		m::log, "Freshness of receipt in %s from %s for %s :%s",
		string_view{room_id},
		string_view{user_id},
		string_view{event_id},
		e.what()
	};

	return true;
}

bool
exists__m_receipt_m_read(const m::room::id &room_id,
                         const m::user::id &user_id,
                         const m::event::id &event_id)
{
	const m::user::room user_room
	{
		user_id
	};

	bool ret{false};
	user_room.get(std::nothrow, "ircd.read", room_id, [&ret, &event_id]
	(const m::event &event)
	{
		const auto &content
		{
			at<"content"_>(event)
		};

		ret = unquote(content.get("event_id")) == event_id;
	});

	return ret;
}
