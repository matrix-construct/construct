// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

ircd::string_view
ircd::m::room::origins::random(const mutable_buffer &buf,
                               const closure_bool &proffer)
const
{
	string_view ret;
	const auto closure{[&buf, &proffer, &ret]
	(const string_view &origin)
	{
		ret = { data(buf), copy(buf, origin) };
	}};

	random(closure, proffer);
	return ret;
}

bool
ircd::m::room::origins::random(const closure &view,
                               const closure_bool &proffer)
const
{
	return random(*this, view, proffer);
}

bool
ircd::m::room::origins::random(const origins &origins,
                               const closure &view,
                               const closure_bool &proffer)
{
	bool ret{false};
	const size_t max
	{
		origins.count()
	};

	if(unlikely(!max))
		return ret;

	auto select
	{
		ssize_t(rand::integer(0, max - 1))
	};

	const closure_bool closure{[&proffer, &view, &select]
	(const string_view &origin)
	{
		if(select-- > 0)
			return true;

		// Test if this random selection is "ok" e.g. the callback allows the
		// user to test a blacklist for this origin. Skip to next if not.
		if(proffer && !proffer(origin))
		{
			++select;
			return true;
		}

		view(origin);
		return false;
	}};

	const auto iteration{[&origins, &closure, &ret]
	{
		ret = !origins.for_each(closure);
	}};

	// Attempt select on first iteration
	iteration();

	// If nothing was OK between the random int and the end of the iteration
	// then start again and pick the first OK.
	if(!ret && select >= 0)
		iteration();

	return ret;
}

bool
ircd::m::room::origins::empty()
const
{
	return for_each(closure_bool{[]
	(const string_view &)
	{
		// return false to break and return false.
		return false;
	}});
}

size_t
ircd::m::room::origins::count()
const
{
	size_t ret{0};
	for_each([&ret](const string_view &)
	{
		++ret;
	});

	return ret;
}

size_t
ircd::m::room::origins::count_error()
const
{
	size_t ret{0};
	for_each([&ret](const string_view &server)
	{
		ret += fed::errant(server);
	});

	return ret;
}

size_t
ircd::m::room::origins::count_online()
const
{
	ssize_t ret
	{
		0 - ssize_t(count_error())
	};

	for_each([&ret](const string_view &server)
	{
		ret += bool(fed::exists(server));
	});

	assert(ret >= 0L);
	return std::max(ret, 0L);
}

/// Tests if argument is the only origin in the room.
/// If a zero or more than one origins exist, returns false. If the only origin
/// in the room is the argument origin, returns true.
bool
ircd::m::room::origins::only(const string_view &origin)
const
{
	ushort ret{2};
	for_each(closure_bool{[&ret, &origin]
	(const string_view &origin_) -> bool
	{
		if(origin == origin_)
			ret = 1;
		else
			ret = 0;

		return ret;
	}});

	return ret == 1;
}

bool
ircd::m::room::origins::has(const string_view &origin)
const
{
	db::domain &index
	{
		dbs::room_joined
	};

	char querybuf[dbs::ROOM_JOINED_KEY_MAX_SIZE];
	const auto query
	{
		dbs::room_joined_key(querybuf, room.room_id, origin)
	};

	auto it
	{
		index.begin(query)
	};

	if(!it)
		return false;

	const string_view &key
	{
		lstrip(it->first, "\0"_sv)
	};

	const string_view &key_origin
	{
		std::get<0>(dbs::room_joined_key(key))
	};

	return key_origin == origin;
}

void
ircd::m::room::origins::for_each(const closure &view)
const
{
	for_each(closure_bool{[&view]
	(const string_view &origin)
	{
		view(origin);
		return true;
	}});
}

bool
ircd::m::room::origins::for_each(const closure_bool &view)
const
{
	db::domain &index
	{
		dbs::room_joined
	};

	auto it
	{
		index.begin(room.room_id)
	};

	size_t repeat{0};
	string_view last;
	char lastbuf[rfc1035::NAME_BUFSIZE];
	for(; bool(it); ++it)
	{
		const auto &[origin, user_id]
		{
			dbs::room_joined_key(it->first)
		};

		// This loop is about presenting unique origin strings to our user
		// through the callback. Since we're iterating all members in the room
		// we want to skip members from the same origin after the first member
		// from that origin.
		if(likely(origin != last))
		{
			if(!view(origin))
				return false;

			// Save the witnessed origin string in this buffer for the first
			// member of each origin; also reset the repeat ctr (see below).
			last = { lastbuf, copy(lastbuf, origin) };
			repeat = 0;
			continue;
		};

		// The threshold determines when to incur the cost of a logarithmic
		// seek on a new key. Under this threshold we iterate normally which
		// is a simple pointer-chase to the next record. If this threshold is
		// low, we would pay the logarithmic cost even if every server only had
		// one or two members joined to the room, etc.
		static const size_t repeat_threshold
		{
			6
		};

		// Conditional branch that determines if we should generate a new key
		// to skip many members from the same origin. We do this via increment
		// of the last character of the current origin so the query key is just
		// past the end of all the member records from the last origin.
		if(repeat++ > repeat_threshold)
		{
			assert(!last.empty());
			assert(last.size() < sizeof(lastbuf));
			repeat = 0;
			lastbuf[last.size() - 1]++;
			char keybuf[dbs::ROOM_JOINED_KEY_MAX_SIZE];
			if(!seek(it, dbs::room_joined_key(keybuf, room.room_id, last)))
				break;
		}
	}

	return true;
}
