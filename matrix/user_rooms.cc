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
	const m::room::state state
	{
		user_room
	};

	return state.for_each("ircd.member", [&membership, &closure]
	(const string_view &, const string_view &state_key, const m::event::idx &event_idx)
	{
		bool ret{true};
		m::get(std::nothrow, event_idx, "content", [&state_key, &membership, &closure, &ret]
		(const json::object &content)
		{
			const json::string &membership_
			{
				content.get("membership")
			};

			if(!membership || membership_ == membership)
			{
				const m::room::id &room_id{state_key};
				ret = closure(room_id, membership_);
			}
		});

		return ret;
	});
}
