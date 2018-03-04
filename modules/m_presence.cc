// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

using namespace ircd;

mapi::header
IRCD_MODULE
{
	"Matrix Presence"
};

const string_view
valid_states[]
{
	"online", "offline", "unavailable",
};

extern "C" bool
presence_valid_state(const string_view &state)
{
	return std::any_of(begin(valid_states), end(valid_states), [&state]
	(const string_view &valid)
	{
		return state == valid;
	});
}

extern "C" m::event::id::buf
presence_set(const m::presence &content)
{
	const m::user user
	{
		at<"user_id"_>(content)
	};

	const m::user::room user_room
	{
		user
	};

	return send(user_room, user.user_id, "m.presence", json::strung{content});
}

extern "C" json::object
presence_get(const m::user &user,
             const mutable_buffer &buffer)
{
	const m::user::room user_room
	{
		user
	};

	json::object ret;
	user_room.get(std::nothrow, "m.presence", [&ret, &buffer]
	(const m::event &event)
	{
		const string_view &content
		{
			at<"content"_>(event)
		};

		ret = { data(buffer), copy(buffer, content) };
	});

	return ret;
}
