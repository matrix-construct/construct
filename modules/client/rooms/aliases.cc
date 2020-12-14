// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include "rooms.h"

using namespace ircd;

m::resource::response
get__aliases(client &client,
             const m::resource::request &request,
             const m::room::id &room_id)
{
	if(!m::exists(room_id))
		throw m::NOT_FOUND
		{
			"Cannot find aliases in %s which is not found.",
			string_view{room_id}
		};

	if(!m::visible(room_id, request.user_id))
		throw m::FORBIDDEN
		{
			"You are not allowed to view aliases of %s",
			string_view{room_id},
		};

	const m::room::aliases aliases
	{
		room_id
	};

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

	json::stack::array array
	{
		top, "aliases"
	};

	aliases.for_each([&array]
	(const m::room::alias &room_alias)
	{
		array.append(room_alias);
		return true;
	});

	return std::move(response);
}
