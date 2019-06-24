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
put__tags(client &client,
          const resource::request &request,
          const m::user &user,
          const m::room &room_id);

static resource::response
get__tags(client &client,
          const resource::request &request,
          const m::user &user,
          const m::room &room);

static resource::response
delete__tags(client &client,
             const resource::request &request,
             const m::user &user,
             const m::room &room);

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

resource::response
delete__rooms(client &client,
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

resource::response
put__tags(client &client,
          const resource::request &request,
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

	return resource::response
	{
		client, http::OK
	};
}

resource::response
get__tags(client &client,
          const resource::request &request,
          const m::user &user,
          const m::room &room)
{
	const m::user::room_tags room_tags
	{
		user, room
	};

	resource::response::chunked response
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

resource::response
delete__tags(client &client,
             const resource::request &request,
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

	return resource::response
	{
		client, deleted? http::OK : http::NOT_FOUND
	};
}

bool
IRCD_MODULE_EXPORT
ircd::m::user::room_tags::del(const m::user &user,
                              const m::room &room,
                              const string_view &user_type)
{
	char typebuf[typebuf_size];
	const string_view type
	{
		_type(typebuf, room.room_id)
	};

	const user::room user_room
	{
		user
	};

	const auto event_idx
	{
		user_room.get(std::nothrow, type, user_type)
	};

	if(!event_idx)
		return false;

	const auto event_id
	{
		m::event_id(event_idx)
	};

	redact(user_room, user, event_id, "deleted");
	return true;
}

m::event::id::buf
IRCD_MODULE_EXPORT
ircd::m::user::room_tags::set(const m::user &user,
                              const m::room &room,
                              const string_view &user_type,
                              const json::object &value)
{
	char typebuf[typebuf_size];
	const string_view type
	{
		_type(typebuf, room.room_id)
	};

	const user::room user_room
	{
		user
	};

	return send(user_room, user, type, user_type, value);
}

bool
IRCD_MODULE_EXPORT
ircd::m::user::room_tags::get(std::nothrow_t,
                              const m::user &user,
                              const m::room &room,
                              const string_view &user_type,
                              const closure &closure)
{
	char typebuf[typebuf_size];
	const string_view type
	{
		_type(typebuf, room.room_id)
	};

	const user::room user_room
	{
		user
	};

	const event::idx event_idx
	{
		user_room.get(std::nothrow, type, user_type)
	};

	return event_idx && m::get(std::nothrow, event_idx, "content", [&user_type, &closure]
	(const json::object &content)
	{
		closure(user_type, content);
	});
}

bool
IRCD_MODULE_EXPORT
ircd::m::user::room_tags::for_each(const m::user &user,
                                   const m::room &room,
                                   const closure_bool &closure)
{
	char typebuf[typebuf_size];
	const string_view type
	{
		_type(typebuf, room.room_id)
	};

	static const event::fetch::opts fopts
	{
		event::keys::include {"state_key", "content"}
	};

	const user::room user_room{user};
	const room::state state
	{
		user_room, &fopts
	};

	return state.for_each(type, event::closure_bool{[&closure]
	(const m::event &event)
	{
		const auto &user_type
		{
			at<"state_key"_>(event)
		};

		const auto &content
		{
			json::get<"content"_>(event)
		};

		return closure(user_type, content);
	}});
}

ircd::string_view
IRCD_MODULE_EXPORT
ircd::m::user::room_tags::_type(const mutable_buffer &out,
                                const m::room::id &room_id)
{
	assert(size(out) >= typebuf_size);

	string_view ret;
	ret = strlcpy(out, type_prefix);
	ret = strlcat(out, room_id);
	return ret;
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

	char typebuf[m::event::TYPE_MAX_SIZE];
	const auto &type
	{
		url::decode(typebuf, request.parv[4])
	};

	m::user::room_account_data{user, room}.get(type, [&client]
	(const string_view &type, const json::object &value)
	{
		resource::response
		{
			client, value
		};
	});

	return {}; // responded from closure
}

m::event::id::buf
IRCD_MODULE_EXPORT
ircd::m::user::room_account_data::set(const m::user &user,
                                      const m::room &room,
                                      const string_view &user_type,
                                      const json::object &value)
{
	char typebuf[typebuf_size];
	const string_view type
	{
		_type(typebuf, room.room_id)
	};

	const m::user::room user_room
	{
		user
	};

	return send(user_room, user, type, user_type, value);
}

bool
IRCD_MODULE_EXPORT
ircd::m::user::room_account_data::get(std::nothrow_t,
                                      const m::user &user,
                                      const m::room &room,
                                      const string_view &user_type,
                                      const closure &closure)
{
	char typebuf[typebuf_size];
	const string_view type
	{
		_type(typebuf, room.room_id)
	};

	const user::room user_room{user};
	const room::state state{user_room};
	const event::idx event_idx
	{
		state.get(std::nothrow, type, user_type)
	};

	return event_idx && m::get(std::nothrow, event_idx, "content", [&user_type, &closure]
	(const json::object &content)
	{
		closure(user_type, content);
	});
}

bool
IRCD_MODULE_EXPORT
ircd::m::user::room_account_data::for_each(const m::user &user,
                                           const m::room &room,
                                           const closure_bool &closure)
{
	char typebuf[typebuf_size];
	const string_view type
	{
		_type(typebuf, room.room_id)
	};

	static const event::fetch::opts fopts
	{
		event::keys::include {"state_key", "content"}
	};

	const user::room user_room{user};
	const room::state state
	{
		user_room, &fopts
	};

	return state.for_each(type, event::closure_bool{[&closure]
	(const m::event &event)
	{
		const auto &user_type
		{
			at<"state_key"_>(event)
		};

		const auto &content
		{
			json::get<"content"_>(event)
		};

		return closure(user_type, content);
	}});
}

ircd::string_view
IRCD_MODULE_EXPORT
ircd::m::user::room_account_data::_type(const mutable_buffer &out,
                                        const m::room::id &room_id)
{
	assert(size(out) >= typebuf_size);

	string_view ret;
	ret = strlcpy(out, type_prefix);
	ret = strlcat(out, room_id);
	return ret;
}
