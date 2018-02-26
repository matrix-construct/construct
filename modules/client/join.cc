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
	"Client 7.4.2.3 :Join"
};

resource
join_resource
{
	"/_matrix/client/r0/join/", resource::opts
	{
		"(7.4.2.3) Join room_id or alias.",
		resource::DIRECTORY,
	}
};

static resource::response
_post__join(client &client,
            const resource::request &request,
            const m::room::id &room_id);

static resource::response
_post__join(client &client,
            const resource::request &request,
            const m::room::alias &room_alias);

resource::response
post__join(client &client,
           const resource::request &request)
{
	if(request.parv.size() < 1)
		throw http::error
		{
			http::MULTIPLE_CHOICES, "/join room_id or room_alias required"
		};

	char idbuf[256];
	const auto &id
	{
		url::decode(request.parv[0], idbuf)
	};

	switch(m::sigil(id))
	{
		case m::id::ROOM:
			return _post__join(client, request, m::room::id{id});

		case m::id::ROOM_ALIAS:
			return _post__join(client, request, m::room::alias{id});

		default: throw m::UNSUPPORTED
		{
			"Cannot join a room using a '%s' MXID", reflect(m::sigil(id))
		};
	}
}

resource::method
method_post
{
	join_resource, "POST", post__join,
	{
		method_post.REQUIRES_AUTH
	}
};

static resource::response
_post__join(client &client,
            const resource::request &request,
            const m::room::alias &room_alias)
{
	throw m::NOT_FOUND{}; //TODO: X

	const m::room::id room_id
	{

	};

	return resource::response
	{
		client, json::members
		{
			{ "room_id", room_id }
		}
	};
}

static resource::response
_post__join(client &client,
            const resource::request &request,
            const m::room::id &room_id)
{
	m::join(room_id, request.user_id);

	return resource::response
	{
		client, json::members
		{
			{ "room_id", room_id }
		}
	};
}
