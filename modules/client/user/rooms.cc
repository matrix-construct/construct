// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include "user.h"

using namespace ircd;

static resource::response
put__account_data(client &client,
                  const resource::request &request,
                  const m::user::id &user_id,
                  const m::room::id &room_id)
{
	if(request.parv.size() < 5)
		throw m::NEED_MORE_PARAMS
		{
			"type required"
		};

	const string_view &type
	{
		request.parv[4]
	};

	return resource::response
	{
		client, http::OK
	};
}

resource::response
put__rooms(client &client,
           const resource::request &request,
           const m::user::id &user_id)
{
	if(request.parv.size() < 3)
		throw m::NEED_MORE_PARAMS
		{
			"room_id required"
		};

	m::room::id::buf room_id
	{
		url::decode(request.parv[2], room_id)
	};

	if(request.parv.size() < 4)
		throw m::NEED_MORE_PARAMS
		{
			"rooms command required"
		};

	const string_view &cmd
	{
		request.parv[3]
	};

	if(cmd == "account_data")
		return put__account_data(client, request, user_id, room_id);

	throw m::NOT_FOUND
	{
		"/user/rooms/ command not found"
	};
}
