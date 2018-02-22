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
post__join(client &client,
           const resource::request &request,
           const m::room::id &room_id)
{
	const string_view &third_party_signed
	{
		unquote(request["third_party_signed"])
	};

	m::join(room_id, request.user_id);

	return resource::response
	{
		client, json::members
		{
			{ "room_id", room_id }
		}
	};
}
