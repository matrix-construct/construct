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
	"Client 7.1.1 :Create Room"
};

resource
createroom
{
	"/_matrix/client/r0/createRoom",
	{
		"(7.1.1) Create a new room with various configuration options."
	}
};

resource::response
post__createroom(client &client, const resource::request &request)
try
{
	const auto name
	{
		unquote(request["name"])
	};

	const auto visibility
	{
		unquote(request["visibility"])
	};

	const m::id::user sender_id
	{
		request.user_id
	};

	const m::id::room::buf room_id
	{
		m::id::generate, my_host()
	};

	const m::room room
	{
		create(room_id, sender_id)
	};

	const m::event::id::buf join_event_id
	{
		join(room, sender_id)
	};

	return resource::response
	{
		client, http::CREATED,
		{
			{ "room_id", string_view{room_id} }
		}
	};
}
catch(const db::not_found &e)
{
	throw m::error
	{
		http::CONFLICT, "M_ROOM_IN_USE", "The desired room name is in use."
	};

	throw m::error
	{
		http::CONFLICT, "M_ROOM_ALIAS_IN_USE", "An alias of the desired room is in use."
	};
}

resource::method
post_method
{
	createroom, "POST", post__createroom,
	{
		post_method.REQUIRES_AUTH
	}
};
