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
	if(request.parv.size() < 1)
		throw m::NEED_MORE_PARAMS
		{
			"Room alias path parameter missing"
		};

	m::room::alias::buf room_alias
	{
		url::decode(room_alias, request.parv[0])
	};

	const m::room::id &room_id
	{
		unquote(request.at("room_id"))
	};

	if(!exists(room_id))
		throw m::NOT_FOUND
		{
			"Room %s is not found here.",
			string_view{room_id}
		};

	const m::room::power power{room_id};
	if(!power(request.user_id, "", "m.room.aliases", room_alias.host()))
		throw m::ACCESS_DENIED
		{
			"Insufficient power in %s to set alias %s",
			string_view{room_id},
			string_view{room_alias}
		};

	if(m::room::aliases::cache::has(room_alias))
		throw m::error
		{
			http::CONFLICT, "M_EXISTS",
			"Room alias %s already exists",
			room_alias
		};

	const unique_buffer<mutable_buffer> buf
	{
		4_KiB //TODO: conf
	};

	json::stack out{buf};
	json::stack::object content{out};
	json::stack::array array
	{
		content, "aliases"
	};

	const m::room::aliases aliases{room_id};
	aliases.for_each(room_alias.host(), [&array]
	(const m::room::alias &alias)
	{
		array.append(alias);
		return true;
	});

	array.append(room_alias);
	array.~array();
	content.~object();

	const auto eid
	{
		m::send(room_id, request.user_id, "m.room.aliases", room_alias.host(), json::object
		{
			out.completed()
		})
	};

	return resource::response
	{
		client, json::members
		{
			{ "event_id", eid }
		}
	};
}
