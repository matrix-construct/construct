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
get_rooms(client &client, const resource::request &request)
{
	if(request.parv.size() < 2)
		throw m::NEED_MORE_PARAMS
		{
			"/rooms command required"
		};

	m::room::id::buf room_id
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

	throw m::NOT_FOUND
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
put_rooms(client &client, const resource::request &request)
{
	if(request.parv.size() < 2)
		throw m::NEED_MORE_PARAMS
		{
			"/rooms command required"
		};

	m::room::id::buf room_id
	{
		url::decode(request.parv[0], room_id)
	};

	const string_view &cmd
	{
		request.parv[1]
	};

	if(cmd == "send")
		return put__send(client, request, room_id);

	if(cmd == "typing")
		return put__typing(client, request, room_id);

	if(cmd == "redact")
		return put__redact(client, request, room_id);

	throw m::NOT_FOUND
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
		throw m::NEED_MORE_PARAMS
		{
			"/rooms command required"
		};

	m::room::id::buf room_id
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

	if(cmd == "redact")
		return post__redact(client, request, room_id);

	throw m::NOT_FOUND
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
