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
get__members(client &client,
             const resource::request &request,
             const m::room::id &room_id)
{
	const m::room::members members
	{
		room_id
	};

	std::vector<json::value> ret;
	ret.reserve(32);

	members.for_each([&ret](const m::event &event)
	{
		ret.emplace_back(event);
	});

	return resource::response
	{
		client, json::members
		{
			{ "chunk", json::value { ret.data(), ret.size() } }
		}
	};
}

resource::response
get__joined_members(client &client,
                    const resource::request &request,
                    const m::room::id &room_id)
{
	const m::room::members members
	{
		room_id
	};

	std::vector<json::member> ret;
	ret.reserve(32);

	members.for_each("join", [&ret](const m::event &event)
	{
		ret.emplace_back(json::member
		{
			at<"sender"_>(event), at<"content"_>(event)
		});
	});

	const json::strung joined
	{
		ret.data(), ret.data() + ret.size()
	};

	return resource::response
	{
		client, json::members
		{
			{ "joined", joined }
		}
	};
}
