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
ircd::m::user::servers::has(const string_view &server,
                            const string_view &membership)
const
{
	// Return true if broken out of loop.
	return !for_each(membership, []
	(const auto &server)
	{
		// Break out of loop at first shared room
		return false;
	});
}

size_t
ircd::m::user::servers::count(const string_view &membership)
const
{
	size_t ret{0};
	for_each(membership, [&ret]
	(const auto &server)
	{
		++ret;
		return true;
	});

	return ret;
}

bool
ircd::m::user::servers::for_each(const closure_bool &closure)
const
{
	return for_each(string_view{}, closure);
}

bool
ircd::m::user::servers::for_each(const string_view &membership,
                                 const closure_bool &closure)
const
{
	const m::user::rooms rooms
	{
		user
	};

	std::set<std::string, std::less<>> seen;
	return rooms.for_each(membership, rooms::closure_bool{[&closure, &seen]
	(const m::room &room, const string_view &membership)
	{
		const room::origins origins
		{
			room
		};

		return origins.for_each(room::origins::closure_bool{[&closure, &seen]
		(const string_view &origin)
		{
			const auto it
			{
				seen.lower_bound(origin)
			};

			if(it != end(seen) && *it == origin)
				return true;

			seen.emplace_hint(it, std::string{origin});
			return closure(origin);
		}});
	}});
}
