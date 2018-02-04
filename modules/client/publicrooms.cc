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

resource publicrooms_resource
{
	"/_matrix/client/r0/publicRooms",
	{
		"Lists the public rooms on the server. "
		"This API returns paginated responses. (7.5)"
	}
};

const ircd::m::room::id::buf
public_room_id
{
    "public", ircd::my_host()
};

m::room public_
{
	public_room_id
};

resource::response
get_publicrooms(client &client, const resource::request &request)
{
	return resource::response
	{
		client, http::OK
	};
}

resource::method post
{
	publicrooms_resource, "GET", get_publicrooms
};

mapi::header IRCD_MODULE
{
	"registers the resource 'client/publicrooms' to manage Matrix rooms"
};
