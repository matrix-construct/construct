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
	"Client 8.1 :User Directory"
};

resource
search_resource
{
	"/_matrix/client/r0/user_directory/search",
	{
		"(8.1) User directory search",
	}
};

resource::response
post__search(client &client,
             const resource::request &request)
{
	const json::string &search_term
	{
		request.at("search_term")
	};

	const ushort &limit
	{
		request.get<ushort>("limit", 10)
	};

	// Search term in this endpoint comes in as-is from Riot. Our query
	// is a lower_bound of a user_id, so we have to prefix the '@'.
	char qbuf[256] {"@"};
	const string_view &query
	{
		startswith(search_term, ':')?
			string_view{search_term}:
		!startswith(search_term, '@')?
			string_view{ircd::strlcat{qbuf, search_term}}:
			string_view{search_term}
	};

	const unique_buffer<mutable_buffer> buf
	{
		4_KiB
	};

	json::stack out{buf};
	json::stack::object top{out};

	size_t count(0);
	bool limited{false};
	{
		json::stack::array results
		{
			top, "results"
		};

		const m::users::opts opts{query};
		m::users::for_each(opts, [&results, &limit, &limited, &count]
		(const m::user::id &user_id)
		{
			json::stack::object result
			{
				results
			};

			json::stack::member
			{
				result, "user_id", user_id
			};

			limited = ++count >= limit;
			return !limited;
		});
	}

	json::stack::member
	{
		top, "limited", json::value{limited}
	};

	top.~object();
	return resource::response
	{
		client, json::object
		{
			out.completed()
		}
	};
}

resource::method
search_post
{
	search_resource, "POST", post__search,
	{
		search_post.REQUIRES_AUTH |
		search_post.RATE_LIMITED
	}
};
