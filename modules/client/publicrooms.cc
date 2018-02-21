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
	"Client 7.5 :Public Rooms"
};

resource
publicrooms_resource
{
	"/_matrix/client/r0/publicRooms",
	{
		"(7.5) Lists the public rooms on the server. "
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
get__publicrooms(client &client,
                 const resource::request &request)
{
	return resource::response
	{
		client, http::OK
	};
}

resource::response
post__publicrooms(client &client,
                  const resource::request &request)
{
	return resource::response
	{
		client, http::OK
	};
}

resource::method
get_method
{
	publicrooms_resource, "GET", get__publicrooms
};

resource::method
post_method
{
	publicrooms_resource, "POST", post__publicrooms
};
