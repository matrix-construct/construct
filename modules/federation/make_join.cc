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
	"Federation :Request a prototype for creating a join event."
};

const string_view
make_join_description
{R"(

Sends a partial event to the remote with enough information for them to
create a join event 'in the blind' for one of their users.

)"};

resource
make_join_resource
{
	"/_matrix/federation/v1/make_join/",
	{
		make_join_description,
		resource::DIRECTORY
	}
};

resource::response
get__make_join(client &client,
                const resource::request &request)
{
	if(request.parv.size() < 1)
		throw m::NEED_MORE_PARAMS
		{
			"room_id path parameter required"
		};

	m::room::id::buf room_id
	{
		url::decode(room_id, request.parv[0])
	};

	if(request.parv.size() < 2)
		throw m::NEED_MORE_PARAMS
		{
			"user_id path parameter required"
		};

	m::user::id::buf user_id
	{
		url::decode(user_id, request.parv[1])
	};

	int64_t depth;
	m::id::event::buf prev_event_id;
	std::tie(prev_event_id, depth, std::ignore) = m::top(room_id);

	const m::event::fetch evf
	{
		prev_event_id, std::nothrow
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

	const m::room::state state
	{
		room_id
	};

	auto auth_event_id
	{
		state.get(std::nothrow, "m.room.member", user_id)
	};

	if(!auth_event_id)
		auth_event_id = state.get("m.room.create");

	const m::event::fetch aevf
	{
		auth_event_id, std::nothrow
	};

	const json::value auth[]
	{
		{ string_view{auth_event_id}  },
		{ json::get<"hashes"_>(aevf) }
	};

	const json::value auths[]
	{
		{ auth, 2 }
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
		{ event,    { "state_key",         user_id                   }},
		{ event,    { "sender",            user_id                   }},
		{ event,    { "depth",             depth + 1                 }},
		{ event,    { "membership",        "join"                    }},
		{ content,  { "membership",        "join"                    }},
		{ event,    { "auth_events",       { auths, 1 }              }},
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
	make_join_resource, "GET", get__make_join,
	{
		method_get.VERIFY_ORIGIN
	}
};
