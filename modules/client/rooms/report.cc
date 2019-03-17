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

static void create_report_room(const m::event &, m::vm::eval &);

const m::room::id::buf
report_room_id
{
	"abuse", m::my_host()
};

conf::item<size_t>
reason_max
{
	{ "name",    "ircd.client.rooms.report.reason.max" },
	{ "default",  512L                                 }
};

const m::hookfn<m::vm::eval &>
create_report_room_hook
{
	create_report_room,
	{
		{ "_site",       "vm.effect"      },
		{ "room_id",     "!ircd"          },
		{ "type",        "m.room.create"  },
	}
};

resource::response
post__report(client &client,
             const resource::request &request,
             const m::room::id &room_id)
{
	if(request.parv.size() < 3)
		throw m::NEED_MORE_PARAMS
		{
			"event_id path parameter required"
		};

	if(!m::exists(room_id))
		throw m::NOT_FOUND
		{
			"Cannot take a report about %s which is not found.",
			string_view{room_id}
		};

	m::event::id::buf event_id
	{
		url::decode(event_id, request.parv[2])
	};

	if(!m::exists(event_id))
		throw m::NOT_FOUND
		{
			"Cannot take a report about %s which is not found.",
			string_view{event_id}
		};

	const short &score
	{
		request.get<short>("score")
	};

	const json::string &reason
	{
		request.at("reason")
	};

	const m::room room
	{
		report_room_id
	};

	if(!exists(room))
		throw m::UNAVAILABLE
		{
			"Sorry, reporting content is not available right now."
		};

	send(room, request.user_id, "ircd.reported",
	{
		{ "room_id",     room_id                           },
		{ "event_id",    event_id                          },
		{ "score",       score                             },
		{ "reason",      trunc(reason, size_t(reason_max)) },
	});

	return resource::response
	{
		client, http::OK
	};
}

void
create_report_room(const m::event &,
                   m::vm::eval &)
try
{
	if(m::exists(report_room_id))
		return;

	const m::room room
	{
		m::create(report_room_id, m::me, "internal")
	};

	log::debug
	{
		m::log, "Created '%s' for the client report module.",
		string_view{room.room_id}
	};
}
catch(const std::exception &e)
{
	log::critical
	{
		m::log, "Creating the '%s' room failed :%s",
		string_view{report_room_id},
		e.what()
	};
}
