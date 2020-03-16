// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

ircd::m::event::id::buf
ircd::m::user::account_data::set(const string_view &type,
                                 const json::object &value)
const
{
	const m::user::room user_room
	{
		user
	};

	return send(user_room, user, "ircd.account_data", type, value);
}

ircd::json::object
ircd::m::user::account_data::get(const mutable_buffer &out,
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
ircd::m::user::account_data::get(const string_view &type,
                                 const closure &closure)
const
{
	if(!get(std::nothrow, type, closure))
		throw m::NOT_FOUND
		{
			"account data type '%s' for user %s not found",
			type,
			string_view{user.user_id}
		};
}

bool
ircd::m::user::account_data::get(std::nothrow_t,
                                 const string_view &type,
                                 const closure &closure)
const
{
	const user::room user_room{user};
	const room::state state{user_room};
	const event::idx &event_idx
	{
		state.get(std::nothrow, "ircd.account_data", type)
	};

	return event_idx && m::get(std::nothrow, event_idx, "content", [&type, &closure]
	(const json::object &content)
	{
		closure(type, content);
	});
}

bool
ircd::m::user::account_data::for_each(const closure_bool &closure)
const
{
	static const event::fetch::opts fopts
	{
		event::keys::include {"state_key", "content"}
	};

	const user::room user_room{user};
	const room::state state
	{
		user_room, &fopts
	};

	return state.for_each("ircd.account_data", event::closure_bool{[&closure]
	(const m::event &event)
	{
		const auto &key
		{
			at<"state_key"_>(event)
		};

		const auto &val
		{
			json::get<"content"_>(event)
		};

		return closure(key, val);
	}});
}
