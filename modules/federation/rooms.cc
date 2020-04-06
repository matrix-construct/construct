// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

using namespace ircd;

m::resource::response
get__rooms__complexity(client &client,
                       const m::resource::request &request,
                       const m::room::id &room_id);

m::resource::response
get__rooms(client &client,
           const m::resource::request &request);

mapi::header
IRCD_MODULE
{
	"Federation Rooms (undocumented)"
};

m::resource
rooms_resource
{
	"/_matrix/federation/unstable/rooms/",
	{
		"federation rooms (unstable) (undocumented)",
		resource::DIRECTORY,
	}
};

m::resource::method
method_get
{
	rooms_resource, "GET", get__rooms,
	{
		method_get.VERIFY_ORIGIN
	}
};

m::resource::response
get__rooms(client &client,
           const m::resource::request &request)
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

	if(m::room::server_acl::enable_read && !m::room::server_acl::check(room_id, request.node_id))
		throw m::ACCESS_DENIED
		{
			"You are not permitted by the room's server access control list."
		};

	if(request.parv.size() < 2)
		throw m::NEED_MORE_PARAMS
		{
			"operation path parameter required"
		};

	char command_buf[128];
	const string_view command
	{
		url::decode(command_buf, request.parv[1])
	};

	if(command == "complexity")
		return get__rooms__complexity(client, request, room_id);

	throw m::NOT_FOUND
	{
		"Unknown federation rooms command"
	};
}

m::resource::response
get__rooms__complexity(client &client,
                       const m::resource::request &request,
                       const m::room::id &room_id)
{
	const float complexity
	{
		0.00
	};

	return resource::response
	{
		client, json::members
		{
			{ "v1", complexity }
		}
	};
}
