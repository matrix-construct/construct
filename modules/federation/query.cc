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

m::resource
query_resource
{
	"/_matrix/federation/v1/query/",
	{
		query_description,
		resource::DIRECTORY
	}
};

static m::resource::response
get__query_profile(client &client,
                   const m::resource::request &request);

static m::resource::response
get__query_directory(client &client,
                     const m::resource::request &request);

m::resource::response
get__query(client &client,
           const m::resource::request &request)
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

m::resource::method
method_get
{
	query_resource, "GET", get__query,
	{
		method_get.VERIFY_ORIGIN
	}
};

m::resource::response
get__query_profile(client &client,
                   const m::resource::request &request)
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
			m::resource::response
			{
				client, json::members
				{
					{ field, value }
				}
			};
		});

		return {};
	}

	m::resource::response::chunked response
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

	return std::move(response);
}

m::resource::response
get__query_directory(client &client,
                     const m::resource::request &request)
{
	m::room::alias::buf room_alias
	{
		url::decode(room_alias, request.query.at("room_alias"))
	};

	const auto room_id
	{
		m::room_id(room_alias)
	};

	const unique_buffer<mutable_buffer> buf
	{
		4_KiB
	};

	json::stack out{buf};
	json::stack::object top
	{
		out
	};

	json::stack::member
	{
		top, "room_id", room_id
	};

	json::stack::array servers
	{
		top, "servers"
	};

	servers.append(my_host());
	if(visible(m::room(room_id), request.node_id))
	{
		static const size_t max_servers
		{
			size(buf) / rfc1035::NAME_MAX - 2
		};

		const m::room::origins origins
		{
			room_id
		};

		size_t i(0);
		origins.for_each(m::room::origins::closure_bool{[&i, &servers]
		(const string_view &origin)
		{
			if(my_host(origin))
				return true;

			if(!server::exists(m::fed::matrix_service(origin)))
				return true;

			servers.append(origin);
			return ++i < max_servers;
		}});
	}

	servers.~array();
	top.~object();
	return m::resource::response
	{
		client, json::object
		{
			out.completed()
		}
	};
}
