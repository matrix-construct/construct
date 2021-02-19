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

m::resource
join_resource
{
	"/_matrix/client/r0/join/",
	{
		"(7.4.2.3) Join room_id or alias.",
		resource::DIRECTORY,
	}
};

static m::resource::response
_post__join(client &client,
            const m::resource::request &request,
            const m::room::id &room_id,
            const vector_view<const string_view> &server_names);

static m::resource::response
_post__join(client &client,
            const m::resource::request &request,
            const m::room::alias &room_alias,
            const vector_view<const string_view> &server_names);

m::resource::response
post__join(client &client,
           const m::resource::request &request)
{
	if(request.parv.size() < 1)
		throw http::error
		{
			http::MULTIPLE_CHOICES, "/join room_id or room_alias required"
		};

	char idbuf[256];
	const auto &id
	{
		url::decode(idbuf, request.parv[0])
	};

	static const size_t server_name_maxarg{16U};
	const auto server_name_count
	{
		std::min(request.query.count("server_name"), server_name_maxarg)
	};

	string_view server_name[server_name_maxarg];
	const unique_mutable_buffer server_name_buf
	{
		rfc3986::DOMAIN_BUFSIZE * server_name_count
	};

	const auto server_names
	{
		request.query.array(server_name_buf, "server_name", server_name)
	};

	switch(m::sigil(id))
	{
		case m::id::ROOM:
			return _post__join(client, request, m::room::id{id}, server_names);

		case m::id::ROOM_ALIAS:
			return _post__join(client, request, m::room::alias{id}, server_names);

		default: throw m::UNSUPPORTED
		{
			"Cannot join a room using a '%s' MXID",
			reflect(m::sigil(id))
		};
	}
}

m::resource::method
method_post
{
	join_resource, "POST", post__join,
	{
		method_post.REQUIRES_AUTH |
		method_post.RATE_LIMITED
	}
};

static m::resource::response
_post__join(client &client,
            const m::resource::request &request,
            const m::room::alias &room_alias,
            const vector_view<const string_view> &server_names)
{
	const auto event_id
	{
		m::join(room_alias, request.user_id)
	};

	return m::resource::response
	{
		client, json::members
		{
			{ "room_id", m::room_id(room_alias) }
		}
	};
}

/// This function forwards the join request to the /rooms/{room_id}/join
/// module so they can reuse the same code. That's done with the m::join()
/// convenience linkage.
static m::resource::response
_post__join(client &client,
            const m::resource::request &request,
            const m::room::id &room_id,
            const vector_view<const string_view> &server_names)
{
	const m::room room
	{
		room_id
	};

	const auto event_id
	{
		m::join(room, request.user_id, server_names)
	};

	return m::resource::response
	{
		client, json::members
		{
			{ "room_id", room_id }
		}
	};
}
