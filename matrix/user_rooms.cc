// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

size_t
IRCD_MODULE_EXPORT
ircd::m::user::rooms::count()
const
{
	size_t ret{0};
	for_each([&ret]
	(const m::room &, const string_view &membership)
	{
		++ret;
	});

	return ret;
}

size_t
IRCD_MODULE_EXPORT
ircd::m::user::rooms::count(const string_view &membership)
const
{
	size_t ret{0};
	for_each(membership, [&ret]
	(const m::room &, const string_view &membership)
	{
		++ret;
	});

	return ret;
}

void
IRCD_MODULE_EXPORT
ircd::m::user::rooms::for_each(const closure &closure)
const
{
	for_each(closure_bool{[&closure]
	(const m::room &room, const string_view &membership)
	{
		closure(room, membership);
		return true;
	}});
}

bool
IRCD_MODULE_EXPORT
ircd::m::user::rooms::for_each(const closure_bool &closure)
const
{
	return for_each(string_view{}, closure);
}

void
IRCD_MODULE_EXPORT
ircd::m::user::rooms::for_each(const string_view &membership,
                               const closure &closure)
const
{
	for_each(membership, closure_bool{[&closure]
	(const m::room &room, const string_view &membership)
	{
		closure(room, membership);
		return true;
	}});
}

bool
IRCD_MODULE_EXPORT
ircd::m::user::rooms::for_each(const string_view &membership,
                               const closure_bool &closure)
const
{
	m::events::state::tuple query;
	std::get<0>(query) = user;
	std::get<1>(query) = "m.room.member";

	m::room::id::buf room_id_;
	return m::events::state::for_each(query, [&]
	(const auto &tuple)
	{
		bool ret{true};
		const auto &[state_key, type, room_id, depth, event_idx]
		{
			tuple
		};

		assert(type == "m.room.member");
		assert(state_key == user);
		if(room_id != room_id_)
			room_id_ = room_id;
		else
			return ret;

		const auto &_room_id(room_id);
		m::get(std::nothrow, event_idx, "content", [&]
		(const json::object &content)
		{
			const json::string &_membership
			{
				content["membership"]
			};

			if(!membership || _membership == membership)
				ret = closure(_room_id, _membership);
		});

		return ret;
	});
}
