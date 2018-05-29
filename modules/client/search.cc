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
	"Client 11.14 :Server Side Search"
};

resource
search
{
	"/_matrix/client/r0/search",
	{
		"(11.14.1) The search API allows clients to perform full text search"
		" across events in all rooms that the user has been in, including"
		" those that they have left. Only events that the user is allowed to"
		" see will be searched, e.g. it won't include events in rooms that"
		" happened after you left."
	}
};

resource::response
post__search(client &client, const resource::request &request)
{
	const auto &batch
	{
		request.query["next_batch"]
	};

	int64_t count(0);
	const string_view next_batch{};

	return resource::response
	{
		client, json::members
		{
			{ "search_categories", json::members
			{
				{ "room_events", json::members
				{
					{ "count",       count          },
					{ "results",     json::array{}  },
					{ "state",       json::object{} },
					{ "groups",      json::object{} },
					{ "next_batch",  next_batch     },
				}}
			}}
		}
	};
}

resource::method
post_method
{
	search, "POST", post__search
};
