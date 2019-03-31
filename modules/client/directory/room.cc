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
	"Client 7.2 :Room aliases"
};

resource
directory_room_resource
{
	"/_matrix/client/r0/directory/room/",
	{
		"(7.2) Room aliases",
		directory_room_resource.DIRECTORY
	}
};

static resource::response
get__directory_room(client &client,
                    const resource::request &request);

resource::method
directory_room_get
{
	directory_room_resource, "GET", get__directory_room
};

resource::response
get__directory_room(client &client,
                    const resource::request &request)
{
	m::room::alias::buf room_alias
	{
		url::decode(room_alias, request.parv[0])
	};

	const m::room::id::buf room_id
	{
		m::room_id(room_alias)
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
put__directory_room(client &client,
                    const resource::request &request);

resource::method
directory_room_put
{
	directory_room_resource, "PUT", put__directory_room
};

resource::response
put__directory_room(client &client,
                    const resource::request &request)
{
	m::room::alias::buf room_alias
	{
		url::decode(room_alias, request.parv[0])
	};

	const m::room::id &room_id
	{
		unquote(request.at("room_id"))
	};

	if(false)
		throw m::error
		{
			http::CONFLICT, "M_EXISTS",
			"Room alias %s already exists",
			room_alias
		};

	return resource::response
	{
		client, http::OK
	};
}
