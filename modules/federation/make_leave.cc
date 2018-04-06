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
	"Federation :Request a prototype for creating a leave event."
};

const string_view
make_leave_description
{R"(

Sends a partial event to the remote with enough information for them to
create a leave event 'in the blind' for one of their users.

)"};

resource
make_leave_resource
{
	"/_matrix/federation/v1/make_leave/",
	{
		make_leave_description,
		resource::DIRECTORY
	}
};

resource::response
get__make_leave(client &client,
                const resource::request &request)
{
	if(request.parv.size() < 1)
		throw m::NEED_MORE_PARAMS
		{
			"room_id path parameter required"
		};

	m::room::id::buf room_id
	{
		url::decode(request.parv[0], room_id)
	};

	if(request.parv.size() < 2)
		throw m::NEED_MORE_PARAMS
		{
			"user_id path parameter required"
		};

	m::user::id::buf user_id
	{
		url::decode(request.parv[1], user_id)
	};

	int64_t depth;
	m::id::event::buf prev_event_id;
	std::tie(depth, prev_event_id) = m::top(room_id);

	const m::event::fetch evf
	{
		prev_event_id, std::nothrow
	};

	const auto &auth_events
	{
		json::get<"auth_events"_>(evf)?
			json::get<"auth_events"_>(evf) : json::array{"[]"}
	};

	const json::value prev[]
	{
		{ string_view{prev_event_id}  },
		{ json::get<"hashes"_>(evf) }
	};

	const json::value prevs[]
	{
		{ prev, 2 }
	};

	thread_local char bufs[96_KiB];
	mutable_buffer buf{bufs};

	json::iov content, event, wrapper;
	const json::iov::push push[]
	{
		{ event,    { "origin",            request.origin            }},
		{ event,    { "origin_server_ts",  time<milliseconds>()      }},
		{ event,    { "room_id",           room_id                   }},
		{ event,    { "type",              "m.room.member"           }},
		{ event,    { "sender",            user_id                   }},
		{ event,    { "state_key",         user_id                   }},
		{ event,    { "depth",             depth + 1                 }},
		{ event,    { "membership",        "leave"                   }},
		{ content,  { "membership",        "leave"                   }},
		{ event,    { "auth_events",       auth_events               }},
		{ event,    { "prev_state",        "[]"                      }},
		{ event,    { "prev_events",       { prevs, 1 }              }},
		{ event,    { "content",           stringify(buf, content)   }},
		{ wrapper,  { "event",             stringify(buf, event)     }},
	};

	return resource::response
	{
		client, wrapper
	};
}

resource::method
method_get
{
	make_leave_resource, "GET", get__make_leave,
	{
		method_get.VERIFY_ORIGIN
	}
};
