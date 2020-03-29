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
	static resource::response get_room_keys_version(client &, const resource::request &);
	extern resource::method room_keys_version_get;

	static resource::response put_room_keys_version(client &, const resource::request &);
	extern resource::method room_keys_version_put;

	static resource::response delete_room_keys_version(client &, const resource::request &);
	extern resource::method room_keys_version_delete;

	static resource::response post_room_keys_version(client &, const resource::request &);
	extern resource::method room_keys_version_post;

	extern resource room_keys_version;
}

ircd::mapi::header
IRCD_MODULE
{
	"Client (undocumented) :e2e Room Keys Version"
};

decltype(ircd::m::room_keys_version)
ircd::m::room_keys_version
{
	"/_matrix/client/unstable/room_keys/version",
	{
		"(undocumented) Room Keys Version",
		resource::DIRECTORY,
	}
};

//
// POST
//

decltype(ircd::m::room_keys_version_post)
ircd::m::room_keys_version_post
{
	room_keys_version, "POST", post_room_keys_version,
	{
		room_keys_version_post.REQUIRES_AUTH
	}
};

ircd::m::resource::response
ircd::m::post_room_keys_version(client &client,
                                const resource::request &request)
{

	return resource::response
	{
		client, http::NOT_IMPLEMENTED
	};
}

//
// DELETE
//

decltype(ircd::m::room_keys_version_delete)
ircd::m::room_keys_version_delete
{
	room_keys_version, "DELETE", delete_room_keys_version,
	{
		room_keys_version_delete.REQUIRES_AUTH
	}
};

ircd::m::resource::response
ircd::m::delete_room_keys_version(client &client,
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

decltype(ircd::m::room_keys_version_put)
ircd::m::room_keys_version_put
{
	room_keys_version, "PUT", put_room_keys_version,
	{
		room_keys_version_put.REQUIRES_AUTH
	}
};

ircd::m::resource::response
ircd::m::put_room_keys_version(client &client,
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

decltype(ircd::m::room_keys_version_get)
ircd::m::room_keys_version_get
{
	room_keys_version, "GET", get_room_keys_version,
	{
		room_keys_version_get.REQUIRES_AUTH
	}
};

ircd::m::resource::response
ircd::m::get_room_keys_version(client &client,
                               const resource::request &request)
{

	return resource::response
	{
		client, http::NOT_IMPLEMENTED
	};
}
