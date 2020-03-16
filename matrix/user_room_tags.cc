// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

bool
ircd::m::user::room_tags::del(const string_view &user_type)
const
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

ircd::m::event::id::buf
ircd::m::user::room_tags::set(const string_view &user_type,
                              const json::object &value)
const
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

ircd::json::object
ircd::m::user::room_tags::get(const mutable_buffer &out,
                              const string_view &type)
const
{
	json::object ret;
	get(std::nothrow, type, [&out, &ret]
	(const string_view &type, const json::object &val)
	{
		ret = string_view { data(out), copy(out, val) };
	});

	return ret;
}

void
ircd::m::user::room_tags::get(const string_view &type,
                              const closure &closure)
const
{
	if(!get(std::nothrow, type, closure))
		throw m::NOT_FOUND
		{
			"account data type '%s' for user %s in room %s not found",
			type,
			string_view{user.user_id},
			string_view{room.room_id}
		};
}

bool
ircd::m::user::room_tags::get(std::nothrow_t,
                              const string_view &user_type,
                              const closure &closure)
const
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
ircd::m::user::room_tags::for_each(const closure_bool &closure)
const
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
ircd::m::user::room_tags::_type(const mutable_buffer &out,
                                const m::room::id &room_id)
{
	assert(size(out) >= typebuf_size);

	string_view ret;
	ret = strlcpy(out, type_prefix);
	ret = strlcat(out, room_id);
	return ret;
}
