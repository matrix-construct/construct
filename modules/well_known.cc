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

mapi::header
IRCD_MODULE
{
	"RFC5785 /.well-known/ support"
};

resource
well_known_resource
{
	"/.well-known/",
	{
		".well-known location handler",
		resource::DIRECTORY
	}
};

static resource::response
handle_matrix(client &client,
             const resource::request &request);

resource::response
get_well_known(client &client,
               const resource::request &request)
{
	if(request.parv.size() > 0)
	{
		if(request.parv[0] == "matrix")
			return handle_matrix(client, request);
	}

	return resource::response
	{
		client, http::NOT_FOUND
	};
}

resource::method
method_get_well_known
{
	well_known_resource, "GET", get_well_known
};

static resource::response
handle_matrix_server(client &client,
                     const resource::request &request);

resource::response
handle_matrix(client &client,
              const resource::request &request)
{
	if(request.parv.size() > 1)
	{
		if(request.parv[1] == "server")
			return handle_matrix_server(client, request);
	}

	return resource::response
	{
		client, http::NOT_FOUND
	};
}

resource::response
handle_matrix_server(client &client,
                     const resource::request &request)
{
	return resource::response
	{
		client, json::members
		{
			{ "m.server", m::self::servername }
		}
	};
}
