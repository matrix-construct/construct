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
room_account_data_get(const m::user &user,
                      const m::room &room,
                      const string_view &type,
                      const m::user::account_data_closure &closure);

extern "C" m::event::id::buf
room_account_data_set(const m::user &user,
                      const m::room &room,
                      const m::user &sender,
                      const string_view &type,
                      const json::object &value);

static resource::response
put__account_data(client &client,
                  const resource::request &request,
                  const m::user &user,
                  const m::room &room_id);

static resource::response
get__account_data(client &client,
                  const resource::request &request,
                  const m::user &user,
                  const m::room &room);

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

resource::response
get__rooms(client &client,
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
		return get__account_data(client, request, user_id, room_id);

	throw m::NOT_FOUND
	{
		"/user/rooms/ command not found"
	};
}

resource::response
put__account_data(client &client,
                  const resource::request &request,
                  const m::user &user,
                  const m::room &room)
{
	if(request.parv.size() < 5)
		throw m::NEED_MORE_PARAMS
		{
			"type path parameter required"
		};

	char typebuf[256];
	const auto &type
	{
		url::decode(request.parv[4], typebuf)
	};

	const json::object &value
	{
		request
	};

	const auto event_id
	{
		room_account_data_set(user, room, user, type, value)
	};

	return resource::response
	{
		client, http::OK
	};
}

resource::response
get__account_data(client &client,
                  const resource::request &request,
                  const m::user &user,
                  const m::room &room)
{

	if(request.parv.size() < 5)
		throw m::NEED_MORE_PARAMS
		{
			"type path parameter required"
		};

	char typebuf[256];
	const auto &type
	{
		url::decode(request.parv[4], typebuf)
	};

	room_account_data_get(user, room, type, [&client]
	(const json::object &value)
	{
		resource::response
		{
			client, value
		};
	});

	return {}; // responded from closure
}

constexpr string_view
room_account_data_type_prefix
{
	"ircd.account_data"_sv
};

constexpr size_t
room_account_data_typebuf_size
{
	m::room::id::MAX_SIZE + size(room_account_data_type_prefix)
};

extern "C" string_view
room_account_data_type(const mutable_buffer &out,
                       const m::room::id &room_id)
{
	assert(size(out) >= room_account_data_typebuf_size);

	string_view ret;
	ret = strlcpy(out, room_account_data_type_prefix);
	ret = strlcat(out, room_id);
	return ret;
}

void
room_account_data_get(const m::user &user,
                      const m::room &room,
                      const string_view &user_type,
                      const m::user::account_data_closure &closure)
try
{
	char typebuf[room_account_data_typebuf_size];
	const string_view type
	{
		room_account_data_type(typebuf, room.room_id)
	};

	const m::user::room user_room
	{
		user
	};

	user_room.get(type, user_type, [&closure]
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
		"Nothing about '%s' account_data for %s in room %s",
		user_type,
		string_view{user.user_id},
		string_view{room.room_id}
	};
}

m::event::id::buf
room_account_data_set(const m::user &user,
                      const m::room &room,
                      const m::user &sender,
                      const string_view &user_type,
                      const json::object &value)
{
	char typebuf[room_account_data_typebuf_size];
	const string_view type
	{
		room_account_data_type(typebuf, room.room_id)
	};

	const m::user::room user_room
	{
		user
	};

	return send(user_room, sender, type, user_type, value);
}
