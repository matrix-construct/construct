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

resource::response
get__initialsync(client &client,
                 const resource::request &request,
                 const m::room::id &room_id)
{
	const string_view membership{};
	const string_view visibility{"public"};
	std::vector<json::value> messages;
	std::vector<json::value> state;
	std::vector<json::value> account_data;

	return resource::response
	{
		client, json::members
		{
			{ "room_id",       room_id                                      },
			{ "membership",    membership                                   },
			{ "messages",      { messages.data(), messages.size() }         },
			{ "state",         { state.data(), state.size() }               },
			{ "visibility",    visibility                                   },
			{ "account_data",  { account_data.data(), account_data.size() } },
		}
	};
}
