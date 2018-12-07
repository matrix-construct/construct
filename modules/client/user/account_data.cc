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

extern "C" void
account_data_get(const m::user &user,
                 const string_view &type,
                 const m::user::account_data_closure &closure);

extern "C" m::event::id::buf
account_data_set(const m::user &user,
                 const m::user &sender,
                 const string_view &type,
                 const json::object &value);

resource::response
put__account_data(client &client,
                  const resource::request &request,
                  const m::user &user)
{
	if(request.parv.size() < 3)
		throw m::NEED_MORE_PARAMS
		{
			"type path parameter required"
		};

	char typebuf[256];
	const string_view type
	{
		url::decode(typebuf, request.parv[2])
	};

	const json::object value
	{
		request
	};

	const auto event_id
	{
		account_data_set(user, user, type, value)
	};

	return resource::response
	{
		client, http::OK
	};
}

resource::response
get__account_data(client &client,
                  const resource::request &request,
                  const m::user &user)
{
	if(request.parv.size() < 3)
		throw m::NEED_MORE_PARAMS
		{
			"type path parameter required"
		};

	char typebuf[256];
	const string_view type
	{
		url::decode(typebuf, request.parv[2])
	};

	account_data_get(user, type, [&client]
	(const json::object &value)
	{
		resource::response
		{
			client, value
		};
	});

	return {}; // responded from closure
}

void
account_data_get(const m::user &user,
                 const string_view &type,
                 const m::user::account_data_closure &closure)
try
{
	const m::user::room user_room
	{
		user
	};

	user_room.get("ircd.account_data", type, [&closure]
	(const m::event &event)
	{
		const json::object &value
		{
			at<"content"_>(event)
		};

		closure(value);
	});
}
catch(const m::NOT_FOUND &e)
{
	throw m::NOT_FOUND
	{
		"Nothing about '%s' account_data for '%s'",
		type,
		string_view{user.user_id}
	};
}

m::event::id::buf
account_data_set(const m::user &user,
                 const m::user &sender,
                 const string_view &type,
                 const json::object &value)
{
	const m::user::room user_room
	{
		user
	};

	return send(user_room, sender, "ircd.account_data", type, value);
}
