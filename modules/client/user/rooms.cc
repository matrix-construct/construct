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

static m::resource::response
put__tags(client &client,
          const m::resource::request &request,
          const m::user &user,
          const m::room &room_id);

static m::resource::response
get__tags(client &client,
          const m::resource::request &request,
          const m::user &user,
          const m::room &room);

static m::resource::response
delete__tags(client &client,
             const m::resource::request &request,
             const m::user &user,
             const m::room &room);

static m::resource::response
put__account_data(client &client,
                  const m::resource::request &request,
                  const m::user &user,
                  const m::room &room_id);

static m::resource::response
get__account_data(client &client,
                  const m::resource::request &request,
                  const m::user &user,
                  const m::room &room);

m::resource::response
put__rooms(client &client,
           const m::resource::request &request,
           const m::user::id &user_id)
{
	if(request.parv.size() < 3)
		throw m::NEED_MORE_PARAMS
		{
			"room_id required"
		};

	m::room::id::buf room_id
	{
		url::decode(room_id, request.parv[2])
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

	if(cmd == "tags")
		return put__tags(client, request, user_id, room_id);

	throw m::NOT_FOUND
	{
		"/user/rooms/ command not found"
	};
}

m::resource::response
get__rooms(client &client,
           const m::resource::request &request,
           const m::user::id &user_id)
{
	if(request.parv.size() < 3)
		throw m::NEED_MORE_PARAMS
		{
			"room_id required"
		};

	m::room::id::buf room_id
	{
		url::decode(room_id, request.parv[2])
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

	if(cmd == "tags")
		return get__tags(client, request, user_id, room_id);

	throw m::NOT_FOUND
	{
		"/user/rooms/ command not found"
	};
}

m::resource::response
delete__rooms(client &client,
              const m::resource::request &request,
              const m::user::id &user_id)
{
	if(request.parv.size() < 3)
		throw m::NEED_MORE_PARAMS
		{
			"room_id required"
		};

	m::room::id::buf room_id
	{
		url::decode(room_id, request.parv[2])
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

	if(cmd == "tags")
		return delete__tags(client, request, user_id, room_id);

	throw m::NOT_FOUND
	{
		"/user/rooms/ command not found"
	};
}

m::resource::response
put__tags(client &client,
          const m::resource::request &request,
          const m::user &user,
          const m::room &room)
{
	if(request.parv.size() < 5)
		throw m::NEED_MORE_PARAMS
		{
			"tag path parameter required"
		};

	char tagbuf[m::event::TYPE_MAX_SIZE];
	const auto &tag
	{
		url::decode(tagbuf, request.parv[4])
	};

	const json::object &value
	{
		request
	};

	const m::user::room_tags room_tags
	{
		user, room
	};

	room_tags.set(tag, value);

	return m::resource::response
	{
		client, http::OK
	};
}

m::resource::response
get__tags(client &client,
          const m::resource::request &request,
          const m::user &user,
          const m::room &room)
{
	const m::user::room_tags room_tags
	{
		user, room
	};

	m::resource::response::chunked response
	{
		client, http::OK
	};

	json::stack out
	{
		response.buf, response.flusher()
	};

	json::stack::object top{out};
	json::stack::object tags
	{
		top, "tags"
	};

	room_tags.for_each([&tags]
	(const string_view &type, const json::object &content)
	{
		json::stack::member
		{
			tags, type, content
		};

		return true;
	});

	return std::move(response);
}

m::resource::response
delete__tags(client &client,
             const m::resource::request &request,
             const m::user &user,
             const m::room &room)
{

	if(request.parv.size() < 5)
		throw m::NEED_MORE_PARAMS
		{
			"tag path parameter required"
		};

	char tagbuf[m::event::TYPE_MAX_SIZE];
	const auto &tag
	{
		url::decode(tagbuf, request.parv[4])
	};

	const m::user::room_tags room_tags
	{
		user, room
	};

	const bool deleted
	{
		room_tags.del(tag)
	};

	return m::resource::response
	{
		client, http::OK
	};
}

m::resource::response
put__account_data(client &client,
                  const m::resource::request &request,
                  const m::user &user,
                  const m::room &room)
{
	if(request.parv.size() < 5)
		throw m::NEED_MORE_PARAMS
		{
			"type path parameter required"
		};

	char typebuf[m::event::TYPE_MAX_SIZE];
	const auto &type
	{
		url::decode(typebuf, request.parv[4])
	};

	const json::object &value
	{
		request
	};

	const auto event_id
	{
		m::user::room_account_data{user, room}.set(type, value)
	};

	return m::resource::response
	{
		client, http::OK
	};
}

m::resource::response
get__account_data(client &client,
                  const m::resource::request &request,
                  const m::user &user,
                  const m::room &room)
{

	if(request.parv.size() < 5)
		throw m::NEED_MORE_PARAMS
		{
			"type path parameter required"
		};

	char typebuf[m::event::TYPE_MAX_SIZE];
	const auto &type
	{
		url::decode(typebuf, request.parv[4])
	};

	m::user::room_account_data{user, room}.get(type, [&client]
	(const string_view &type, const json::object &value)
	{
		m::resource::response
		{
			client, value
		};
	});

	return {}; // responded from closure
}
