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

resource directory_room_resource
{
	"/_matrix/client/r0/directory/room",
	{
		"(7.2) Room aliases",
		directory_room_resource.DIRECTORY
	}
};

resource::response
get_directory_room(client &client, const resource::request &request)
{
	m::room::alias::buf room_alias
	{
		url::decode(request.parv[0], room_alias)
	};

	return resource::response
	{
		client, http::NOT_FOUND
	};
}

resource::method directory_room_get
{
	directory_room_resource, "GET", get_directory_room
};

mapi::header IRCD_MODULE
{
	"registers the resource 'client/directory/room' to manage the Matrix room directory"
};
