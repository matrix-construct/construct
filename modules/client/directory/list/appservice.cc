// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

using namespace ircd;

static resource::response
put__list_appservice(client &client,
                     const resource::request &request);

mapi::header
IRCD_MODULE
{
	"Application Service 2.3.5 :Application service room directories"
};

resource
list_appservice_resource
{
	"/_matrix/client/r0/directory/list/appservice/",
	{
		"(AS 2.3.5) Application service room directories",
		list_appservice_resource.DIRECTORY
	}
};

resource::method
list_appservice_put
{
	list_appservice_resource, "PUT", put__list_appservice,
	{
		list_appservice_put.REQUIRES_AUTH
	}
};

resource::response
put__list_appservice(client &client,
                     const resource::request &request)
{
	if(request.parv.size() < 1)
		throw m::NEED_MORE_PARAMS
		{
			"network_id path parameter required"
		};

	char network_id_buf[256];
	const string_view &network_id
	{
		url::decode(network_id_buf, request.parv[0])
	};

	if(request.parv.size() < 2)
		throw m::NEED_MORE_PARAMS
		{
			"room_id path parameter required"
		};

	m::room::id::buf room_id
	{
		url::decode(room_id, request.parv[1])
	};

	const m::room room
	{
		room_id
	};

	if(!exists(room))
		throw m::NOT_FOUND
		{
			"Room %s is not known to this server",
			string_view{room_id}
		};

	const bool permitted
	{
		//TODO: XXX
		false
	};

	if(!permitted)
		throw m::ACCESS_DENIED
		{
			"You do not have permission to list the room on this server"
		};

	const json::string &visibility
	{
		request.at("visibility")
	};

	switch(hash(visibility))
	{
		case "public"_:
			// We set an empty summary for this room because
			// we already have its state on this server;
			m::rooms::summary_set(room.room_id, json::object{});
			break;

		case "private"_:
			m::rooms::summary_del(room.room_id);
			break;

		default: throw m::UNSUPPORTED
		{
			"visibility type '%s' is not supported here",
			string_view{visibility}
		};
	}

	return resource::response
	{
		client, http::OK
	};
}
