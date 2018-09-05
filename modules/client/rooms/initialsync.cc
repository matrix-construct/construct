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

conf::item<size_t>
initial_backfill
{
	{ "name",      "ircd.rooms.initialsync.backfill" },
	{ "default",   64L                               }
};

static resource::response
get__initialsync_remote(client &client,
                        const resource::request &request,
                        const m::room::id &room_id)
{
	string_view membership{"join"};
	const string_view visibility{"public"};
	std::vector<m::event> messages;
	std::vector<m::event> state;
	std::vector<json::value> account_data;
	const json::strung chunk
	{
		messages.data(), messages.data() + messages.size()
	};

	const json::strung states
	{
		state.data(), state.data() + state.size()
	};

	return resource::response
	{
		client, json::members
		{
			{ "room_id",       room_id                                        },
			{ "membership",    membership                                     },
			{ "state",         states                                         },
			{ "visibility",    visibility                                     },
			{ "account_data",  { account_data.data(), account_data.size() }   },
			{ "messages",      json::members
			{
				{ "chunk",  chunk }
			}},
		}
	};
}

resource::response
get__initialsync(client &client,
                 const resource::request &request,
                 const m::room::id &room_id)
{
	if(!exists(room_id))
	{
		if(!my(room_id))
			return get__initialsync_remote(client, request, room_id);

		throw m::NOT_FOUND
		{
			"room_id '%s' does not exist", string_view{room_id}
		};
	}

	const m::room room{room_id};
	const m::room::state state
	{
		room
	};

	string_view membership{"join"};
	const string_view visibility{"public"};
	std::vector<m::event> messages;
	std::vector<m::event> statev;
	std::vector<json::value> account_data;
	const string_view messages_start{};
	const string_view messages_end{};
	const json::strung chunk
	{
		messages.data(), messages.data() + messages.size()
	};

	const json::strung states
	{
		statev.data(), statev.data() + statev.size()
	};

	const json::strung account_datas
	{
		account_data.data(), account_data.data() + account_data.size()
	};

	return resource::response
	{
		client, json::members
		{
			{ "room_id",       room_id        },
			{ "membership",    membership     },
			{ "state",         states,        },
			{ "visibility",    visibility     },
			{ "account_data",  account_datas  },
			{ "messages",      json::members
			{
				{ "start", messages_start },
				{ "chunk", chunk },
				{ "end", messages_end },
			}},
		}
	};
}
