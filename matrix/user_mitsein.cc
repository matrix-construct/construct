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
ircd::m::user::mitsein::has(const m::user &other,
                            const string_view &membership)
const
{
	// Return true if broken out of loop.
	return !for_each(other, membership, []
	(const m::room &, const string_view &)
	{
		// Break out of loop at first shared room
		return false;
	});
}

size_t
ircd::m::user::mitsein::count(const string_view &membership)
const
{
	size_t ret{0};
	for_each(membership, [&ret](const m::user &)
	{
		++ret;
		return true;
	});

	return ret;
}

size_t
ircd::m::user::mitsein::count(const m::user &user,
                              const string_view &membership)
const
{
	size_t ret{0};
	for_each(user, membership, [&ret](const m::room &, const string_view &)
	{
		++ret;
		return true;
	});

	return ret;
}

bool
ircd::m::user::mitsein::for_each(const closure_bool &closure)
const
{
	return for_each(string_view{}, closure);
}

bool
ircd::m::user::mitsein::for_each(const string_view &membership,
                                 const closure_bool &closure)
const
{
	const m::user::rooms rooms
	{
		user
	};

	// here we gooooooo :/
	///TODO: ideal: db schema
	///TODO: minimally: custom alloc?
	std::set<uint128_t, std::less<>> seen;
	return rooms.for_each(membership, rooms::closure_bool{[&membership, &closure, &seen]
	(const m::room &room, const string_view &_membership)
	{
		assert(_membership == membership);
		const m::room::members members
		{
			room
		};

		return members.for_each(membership, [&seen, &closure]
		(const user::id &other)
		{
			const auto hash
			{
				uint128_t(ircd::hash(other))
				| (uint128_t(ircd::hash(other.host())) << 64)
			};

			const bool inserted
			{
				seen.emplace(hash).second
			};

			return inserted?
				closure(m::user{other}):
				true;
		});
	}});
}

bool
ircd::m::user::mitsein::for_each(const m::user &user,
                                 const rooms::closure_bool &closure)
const
{
	return for_each(user, string_view{}, closure);
}

bool
ircd::m::user::mitsein::for_each(const m::user &user,
                                 const string_view &membership,
                                 const rooms::closure_bool &closure)
const
{
	const m::user::rooms our_rooms{this->user};
	const m::user::rooms their_rooms{user};
	const bool use_our
	{
		our_rooms.count() <= their_rooms.count()
	};

	const m::user::rooms &rooms
	{
		use_our? our_rooms : their_rooms
	};

	const string_view &test_key
	{
		use_our? user.user_id : this->user.user_id
	};

	return rooms.for_each(membership, rooms::closure_bool{[&membership, &closure, &test_key]
	(const m::room &room, const string_view &)
	{
		if(!room.has("m.room.member", test_key))
			return true;

		return closure(room, membership);
	}});
}
