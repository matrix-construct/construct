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

extern "C" m::id::room
room_id__room_alias(const mutable_buffer &out,
                    const m::id::room_alias &);

resource::response
get__directory_room(client &client,
                    const resource::request &request)
{
	m::room::alias::buf room_alias
	{
		url::decode(request.parv[0], room_alias)
	};

	char buf[256];
	const auto room_id
	{
		m::room_id(buf, room_alias)
	};

	return resource::response
	{
		client, json::members
		{
			{ "room_id", room_id }
		}
	};
}

resource::method
directory_room_get
{
	directory_room_resource, "GET", get__directory_room
};

resource::response
put__directory_room(client &client,
                    const resource::request &request)
{
	m::room::alias::buf room_alias
	{
		url::decode(request.parv[0], room_alias)
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

resource::method
directory_room_put
{
	directory_room_resource, "PUT", put__directory_room
};

m::id::room
room_id__room_alias(const mutable_buffer &out,
                    const m::id::room_alias &alias)
{
	//TODO: XXX cache strat

	const unique_buffer<mutable_buffer> buf
	{
		8_KiB //TODO: XXX
	};

	m::v1::query::directory federation_request
	{
		alias, buf
	};

	//TODO: conf
	if(federation_request.wait(seconds(8)) == ctx::future_status::timeout)
		throw http::error
		{
			http::REQUEST_TIMEOUT
		};

	const http::code &code
	{
		federation_request.get()
	};

	const json::object &response
	{
		federation_request
	};

	if(empty(response["room_id"]))
		throw m::NOT_FOUND{};

	if(empty(response["servers"]))
		throw m::NOT_FOUND{};

	const auto &room_id
	{
		unquote(response.at("room_id"))
	};

	//TODO: XXX cache strat

	return m::room::id
	{
		string_view
		{
			data(out), copy(out, room_id)
		}
	};
}
