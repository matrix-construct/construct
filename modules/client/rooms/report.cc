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

conf::item<size_t>
reason_max
{
	{ "name",    "ircd.client.rooms.report.reason.max" },
	{ "default",  512L                                 }
};

m::resource::response
post__report(client &client,
             const m::resource::request &request,
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
		trunc(request.at("reason"), size_t(reason_max))
	};

	const m::room::id::buf report_room_id
	{
		"abuse", request.user_id.host()
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

	// Include raw report data for bots.
	const json::members report
	{
		{ "room_id",     room_id      },
		{ "event_id",    event_id     },
		{ "score",       score        },
		{ "reason",      reason       },
	};

	// Generate a plaintext summary for text clients.
	const fmt::bsprintf<2_KiB> body
	{
		"Report by %s of %s in %s :%s",
		string_view{request.user_id},
		string_view{event_id},
		string_view{room_id},
		reason,
	};

	const fmt::snstringf formatted_body
	{
		4_KiB,
		R"(
			<h4>Reported %s</h4>
			<blockquote>%s</blockquote>
			https://matrix.to/#/%s/%s
		)",
		string_view{room_id},
		string_view{reason},
		string_view{room_id},
		string_view{event_id},
	};

	// Generate a relates_to/in_reply_to of the event being reported so the
	// censors can be debauched by its content directly in the !abuse room.
	const json::members relates_to[2]
	{
		{ { "event_id",       event_id      } },
		{ { "m.in_reply_to",  relates_to[0] } },
	};

	send(room, request.user_id, "m.room.message",
	{
		{ "msgtype",         "m.notice"                 },
		{ "format",          "org.matrix.custom.html"   },
		{ "formatted_body",  stripa(formatted_body)     },
		{ "body",            body                       },
		{ "m.relates_to",    relates_to[1]              },
		{ "ircd.report",     report                     },
	});

	return m::resource::response
	{
		client, http::OK
	};
}
