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
post__kick(client &client,
           const resource::request &request,
           const m::room::id &room_id)
{
	const m::user::id &user_id
	{
		unquote(request.at("user_id"))
	};

	const string_view &reason
	{
		unquote(request["reason"])
	};

	return resource::response
	{
		client, http::OK
	};
}
