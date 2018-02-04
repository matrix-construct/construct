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

mapi::header IRCD_MODULE
{
	"registers the resource 'client/voip/turnserver' to handle requests."
};

resource turnserver_resource
{
	"/_matrix/client/r0/voip/turnServer",
	{
		"(11.3.3) "
		"This API provides credentials for the client to use when initiating calls."
	}
};

resource::response
get_turnserver(client &client, const resource::request &request)
{
	return resource::response
	{
		client, http::OK
	};
}

resource::method turnserver_get
{
	turnserver_resource, "GET", get_turnserver,
	{
		//get_turnserver.REQUIRES_AUTH
	}
};
