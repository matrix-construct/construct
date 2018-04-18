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

using namespace ircd::m;
using namespace ircd;

mapi::header
IRCD_MODULE
{
	"Client 7 :Rooms"
};

resource
rooms_resource
{
	"/_matrix/client/r0/rooms/",
	{
		"(7.0) Rooms",
		resource::DIRECTORY,
	}
};

resource::response
get_rooms(client &client,
          const resource::request &request)
{
	if(request.parv.size() < 2)
		throw NEED_MORE_PARAMS
		{
			"/rooms command required"
		};

	room::id::buf room_id
	{
		url::decode(request.parv[0], room_id)
	};

	const string_view &cmd
	{
		request.parv[1]
	};

	if(cmd == "messages")
		return get__messages(client, request, room_id);

	if(cmd == "state")
		return get__state(client, request, room_id);

	if(cmd == "members")
		return get__members(client, request, room_id);

	if(cmd == "joined_members")
		return get__joined_members(client, request, room_id);

	if(cmd == "context")
		return get__context(client, request, room_id);

	if(cmd == "initialSync")
		return get__initialsync(client, request, room_id);

	throw NOT_FOUND
	{
		"/rooms command not found"
	};
}

resource::method
method_get
{
	rooms_resource, "GET", get_rooms
};

resource::response
put_rooms(client &client,
          const resource::request &request)
{
	if(request.parv.size() < 2)
		throw NEED_MORE_PARAMS
		{
			"/rooms command required"
		};

	room::id::buf room_id
	{
		url::decode(request.parv[0], room_id)
	};

	const string_view &cmd
	{
		request.parv[1]
	};

	if(cmd == "send")
		return put__send(client, request, room_id);

	if(cmd == "state")
		return put__state(client, request, room_id);

	if(cmd == "typing")
		return put__typing(client, request, room_id);

	if(cmd == "redact")
		return put__redact(client, request, room_id);

	throw NOT_FOUND
	{
		"/rooms command not found"
	};
}

resource::method
method_put
{
	rooms_resource, "PUT", put_rooms,
	{
		method_put.REQUIRES_AUTH
	}
};

resource::response
post_rooms(client &client,
           const resource::request &request)
{
	if(request.parv.size() < 2)
		throw NEED_MORE_PARAMS
		{
			"/rooms command required"
		};

	room::id::buf room_id
	{
		url::decode(request.parv[0], room_id)
	};

	const string_view &cmd
	{
		request.parv[1]
	};

	if(cmd == "read_markers")
		return post__read_markers(client, request, room_id);

	if(cmd == "receipt")
		return post__receipt(client, request, room_id);

	if(cmd == "join")
		return post__join(client, request, room_id);

	if(cmd == "invite")
		return post__invite(client, request, room_id);

	if(cmd == "leave")
		return post__leave(client, request, room_id);

	if(cmd == "forget")
		return post__forget(client, request, room_id);

	if(cmd == "kick")
		return post__kick(client, request, room_id);

	if(cmd == "ban")
		return post__ban(client, request, room_id);

	if(cmd == "unban")
		return post__unban(client, request, room_id);

	if(cmd == "redact")
		return post__redact(client, request, room_id);

	throw NOT_FOUND
	{
		"/rooms command not found"
	};
}

resource::method
method_post
{
	rooms_resource, "POST", post_rooms,
	{
		method_post.REQUIRES_AUTH
	}
};

extern "C" event::id::buf
commit__iov_iov(const room &room,
                json::iov &event,
                const json::iov &contents)
{
	const json::iov::push room_id
	{
		event, { "room_id", room.room_id }
	};

	int64_t depth;
	event::id::buf prev_event_id;
	std::tie(prev_event_id, depth, std::ignore) = m::top(std::nothrow, room.room_id);

	//TODO: X
	const json::iov::set_if depth_
	{
		event, !event.has("depth"),
		{
			"depth", depth + 1
		}
	};

	//TODO: X
	const string_view auth_events{};

	//TODO: X
	const string_view prev_state{};

	//TODO: X
	json::value prev_event0
	{
		prev_event_id, json::STRING
	};

	//TODO: X
	json::value prev_event
	{
		&prev_event0, !empty(prev_event_id)
	};

	//TODO: X
	json::value prev_events
	{
		&prev_event, !empty(prev_event_id)
	};

	//TODO: X
	const json::iov::push prevs[]
	{
		{ event, { "auth_events",  auth_events  } },
		{ event, { "prev_state",   prev_state   } },
		{ event, { "prev_events",  prev_events  } },
	};

	const auto &vmopts
	{
		room.opts? *room.opts : vm::default_commit_opts
	};

	return m::vm::commit(event, contents, vmopts);
}
