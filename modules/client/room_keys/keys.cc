// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m
{
	static resource::response get_room_keys_keys(client &, const resource::request &);
	extern resource::method room_keys_keys_get;

	static resource::response put_room_keys_keys(client &, const resource::request &);
	extern resource::method room_keys_keys_put;

	static resource::response delete_room_keys_keys(client &, const resource::request &);
	extern resource::method room_keys_keys_delete;

	extern resource room_keys_keys;
}

ircd::mapi::header
IRCD_MODULE
{
	"Client (undocumented) :e2e Room Keys Keys"
};

decltype(ircd::m::room_keys_keys)
ircd::m::room_keys_keys
{
	"/_matrix/client/unstable/room_keys/keys",
	{
		"(undocumented) Room Keys Keys",
		resource::DIRECTORY,
	}
};

//
// DELETE
//

decltype(ircd::m::room_keys_keys_delete)
ircd::m::room_keys_keys_delete
{
	room_keys_keys, "DELETE", delete_room_keys_keys,
	{
		room_keys_keys_delete.REQUIRES_AUTH
	}
};

ircd::m::resource::response
ircd::m::delete_room_keys_keys(client &client,
                               const resource::request &request)
{

	return resource::response
	{
		client, http::NOT_IMPLEMENTED
	};
}

//
// PUT
//

decltype(ircd::m::room_keys_keys_put)
ircd::m::room_keys_keys_put
{
	room_keys_keys, "PUT", put_room_keys_keys,
	{
		room_keys_keys_put.REQUIRES_AUTH
	}
};

ircd::m::resource::response
ircd::m::put_room_keys_keys(client &client,
                            const resource::request &request)
{

	return resource::response
	{
		client, http::NOT_IMPLEMENTED
	};
}

//
// GET
//

decltype(ircd::m::room_keys_keys_get)
ircd::m::room_keys_keys_get
{
	room_keys_keys, "GET", get_room_keys_keys,
	{
		room_keys_keys_get.REQUIRES_AUTH
	}
};

ircd::m::resource::response
ircd::m::get_room_keys_keys(client &client,
                            const resource::request &request)
{

	return resource::response
	{
		client, http::NOT_IMPLEMENTED
	};
}
