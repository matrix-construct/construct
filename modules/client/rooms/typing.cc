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

conf::item<milliseconds>
timeout_default
{
	{ "name",     "ircd.typing.timeout.default" },
	{ "default",  30 * 1000L                    },
};

resource::response
put__typing(client &client,
            const resource::request &request,
            const m::room::id &room_id)
{
	if(request.parv.size() < 3)
		throw m::NEED_MORE_PARAMS
		{
			"user_id parameter missing"
		};

	m::user::id::buf user_id
	{
		url::decode(user_id, request.parv[2])
	};

	if(request.user_id != user_id)
		throw m::UNSUPPORTED
		{
			"Typing as someone else not yet supported"
		};

	const bool typing
	{
		request.get("typing", false)
	};

	const time_t timeout
	{
		request.get("timeout", milliseconds(timeout_default).count())
	};

	const m::typing event
	{
		{ "room_id",  room_id   },
		{ "typing",   typing    },
		{ "user_id",  user_id   },
		{ "timeout",  timeout   },
	};

	m::typing::commit
	{
		event
	};

	return resource::response
	{
		client, http::OK
	};
}
