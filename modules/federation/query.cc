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
	"Federation :Query"
};

const string_view
query_description
{R"(
Performs a single query request on the receiving homeserver. 
The Query Type part of the path specifies the kind of query 
being made, and its query arguments have a meaning specific to 
that kind of query. The response is a JSON-encoded object whose 
meaning also depends on the kind of query.
)"};

resource
query_resource
{
	"/_matrix/federation/v1/query/",
	{
		query_description,
		resource::DIRECTORY
	}
};

static resource::response
get__query_profile(client &client,
                   const resource::request &request);

resource::response
get__query(client &client,
           const resource::request &request)
{
	const auto type
	{
		request.parv[0]
	};

	if(type == "profile")
		return get__query_profile(client, request);

	throw m::NOT_FOUND
	{
		"Query type not found."
	};
}

resource::method
method_get
{
	query_resource, "GET", get__query,
	{
		method_get.VERIFY_ORIGIN
	}
};

resource::response
get__query_profile(client &client,
                   const resource::request &request)
{
	m::user::id::buf user_id
	{
		url::decode(request.query.at("user_id"), user_id)
	};

	const string_view field
	{
		//TODO: XXX full profile aggregation w/ user:: linkage
		request.query.at("field")
	};

	const m::user user
	{
		user_id
	};

	user.profile(field, [&client, &field]
	(const string_view &value)
	{
		resource::response
		{
			client, json::members
			{
				{ field, value }
			}
		};
	});

	return {}; // responded from closure
}
