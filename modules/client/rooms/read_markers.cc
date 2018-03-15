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
post__read_markers(client &client,
                   const resource::request &request,
                   const m::room::id &room_id)
{
	const string_view m_fully_read
	{
		unquote(request["m.fully_read"])
	};

	const string_view m_read
	{
		unquote(request["m.read"])
	};

	m::receipt::read(room_id, request.user_id, m_read?: m_fully_read);

	return resource::response
	{
		client, http::OK
	};
}
