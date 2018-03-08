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

const ircd::m::room::id::buf
alias_room_id
{
    "alias", ircd::my_host()
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

extern "C" bool
room_alias_exists(const m::id::room_alias &,
                  const bool &remote_query);

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

static json::object
room_alias_fetch(const mutable_buffer &out,
                 const m::id::room_alias &alias);

bool
room_alias_exists(const m::id::room_alias &alias,
                  const bool &remote_query)
try
{
	const m::room alias_room{alias_room_id};
	if(alias_room.has("ircd.alias", alias))
		return true;

	if(!remote_query)
		return false;

	char buf[256];
	room_id__room_alias(buf, alias);
	return true;
}
catch(const m::NOT_FOUND &)
{
	return false;
}

/// Translate a room alias into a room_id. This function first checks the
/// local cache. A cache miss will then cause in a query to the remote, the
/// result of which will be added to cache.
m::id::room
room_id__room_alias(const mutable_buffer &out,
                    const m::id::room_alias &alias)
try
{
	m::id::room ret;
	const auto cache_closure{[&out, &ret]
	(const m::event &event)
	{
		const string_view &room_id
		{
			unquote(at<"content"_>(event).get("room_id"))
		};

		if(!room_id)
			return;

		const auto age
		{
			ircd::now() - milliseconds(at<"origin_server_ts"_>(event))
		};

		//TODO: Conf; cache TTL.
		if(age > hours(72))
			return;

		ret = string_view { data(out), copy(out, room_id) };
	}};

	const m::room alias_room{alias_room_id};
	alias_room.get(std::nothrow, "ircd.alias", alias, cache_closure);
	if(ret)
		return ret;

	// Buf has to hold our output headers, their received headers, and
	// the received aliasing content.
	const unique_buffer<mutable_buffer> buf
	{
		32_KiB
	};

	const json::object &response
	{
		room_alias_fetch(buf, alias)
	};

	// Cache the result
	send(alias_room, m::me.user_id, "ircd.alias", alias, response);

	const m::id::room &room_id
	{
		unquote(response.at("room_id"))
	};

	return m::room::id
	{
		string_view
		{
			data(out), copy(out, room_id)
		}
	};
}
catch(const http::error &e)
{
	if(e.code == http::NOT_FOUND)
		throw m::NOT_FOUND{};

	throw;
}

/// This function makes a room alias request to a remote. The alias
/// room cache is not checked or updated from here, this is only the
/// query operation.
json::object
room_alias_fetch(const mutable_buffer &out,
                 const m::id::room_alias &alias)
{
	m::v1::query::directory federation_request
	{
		alias, out
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

	return response;
}

const m::hook
_create_alias_room
{
	{
		{ "_site",       "vm notify"      },
		{ "room_id",     "!ircd"          },
		{ "type",        "m.room.create"  },
	},
	[](const m::event &)
	{
		m::create(alias_room_id, m::me.user_id);
	}
};
