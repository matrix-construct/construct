// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include "rooms.h"

using namespace ircd;

resource::response
get__messages(client &client,
              const resource::request &request,
              const m::room::id &room_id)
{
	const string_view &from
	{
		request.query.at("from")
	};

	const string_view &to
	{
		request.query["to"]
	};

	const char &dir
	{
		request.query.at("dir").at(0)
	};

	const uint16_t limit
	{
		request.query["limit"]? lex_cast<uint16_t>(request.query.at("limit")) : ushort(10)
	};

	const string_view &filter
	{
		request.query["filter"]
	};

	const m::room room
	{
		room_id
	};

	std::vector<json::value> ret;
	ret.reserve(limit);

	m::room::messages it{room};
	for(; it && ret.size() < limit; dir == 'b'? --it : ++it)
		ret.emplace_back(*it);

	const string_view start
	{
		dir == 'b'? from : ":1234"
	};

	const string_view end
	{
		dir == 'b'? ":4321" : string_view{}
	};

	json::value chunk
	{
		ret.data(), ret.size()
	};

	return resource::response
	{
		client, json::members
		{
			{ "start",  start },
			{ "end",    end   },
			{ "chunk",  chunk },
		}
	};
}
