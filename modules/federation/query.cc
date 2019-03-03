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

static resource::response
get__query_directory(client &client,
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

	if(type == "directory")
		return get__query_directory(client, request);

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
		url::decode(user_id, request.query.at("user_id"))
	};

	const string_view field
	{
		request.query["field"]
	};

	const m::user user
	{
		user_id
	};

	const m::user::profile profile
	{
		user
	};

	if(!empty(field))
	{
		profile.get(field, [&client]
		(const string_view &field, const string_view &value)
		{
			resource::response
			{
				client, json::members
				{
					{ field, value }
				}
			};
		});

		return {};
	}

	resource::response::chunked response
	{
		client, http::OK
	};

	json::stack out
	{
		response.buf, response.flusher()
	};

	json::stack::object top
	{
		out
	};

	profile.for_each([&top]
	(const string_view &key, const string_view &val)
	{
		json::stack::member
		{
			top, key, val
		};

		return true;
	});

	return response;
}

resource::response
get__query_directory(client &client,
                     const resource::request &request)
{
	m::room::alias::buf room_alias
	{
		url::decode(room_alias, request.query.at("room_alias"))
	};

	const auto room_id
	{
		m::room_id(room_alias)
	};

	//TODO: servers
	const std::array<json::value, 2> server
	{
		{ room_alias.host(), my_host() }
	};

	const size_t server_count
	{
		server.size() - bool(room_alias.host() == my_host())
	};

	const json::value servers
	{
		server.data(), server_count
	};

	return resource::response
	{
		client, json::members
		{
			{ "room_id", room_id },
			{ "servers", servers },
		}
	};
}
