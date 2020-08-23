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

m::resource::response
put__account_data(client &client,
                  const m::resource::request &request,
                  const m::user &user)
{
	if(request.parv.size() < 3)
		throw m::NEED_MORE_PARAMS
		{
			"type path parameter required"
		};

	char typebuf[m::event::TYPE_MAX_SIZE];
	const string_view type
	{
		url::decode(typebuf, request.parv[2])
	};

	const json::object value
	{
		request
	};

	const m::user::account_data account_data
	{
		user
	};

	const auto event_id
	{
		account_data.set(type, value)
	};

	return m::resource::response
	{
		client, http::OK
	};
}

m::resource::response
get__account_data(client &client,
                  const m::resource::request &request,
                  const m::user &user)
{
	if(request.parv.size() < 3)
		throw m::NEED_MORE_PARAMS
		{
			"type path parameter required"
		};

	char typebuf[m::event::TYPE_MAX_SIZE];
	const string_view type
	{
		url::decode(typebuf, request.parv[2])
	};

	const m::user::account_data account_data
	{
		user
	};

	account_data.get(type, [&client]
	(const string_view &type, const json::object &value)
	{
		resource::response
		{
			client, value
		};
	});

	return {}; // responded from closure
}
