// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

decltype(ircd::m::rooms::opts_default)
ircd::m::rooms::opts_default;

void
ircd::m::rooms::dump__file(const opts &opts,
                           const string_view &filename)
{
	const fs::fd file
	{
		filename, std::ios::out | std::ios::app
	};

	// POSIX_FADV_DONTNEED
	fs::evict(file);

	size_t len(0), num(0);
	for_each(opts, [&](const auto &room_id)
	{
		const const_buffer bufs[]
		{
			room_id, "\n"_sv
		};

		len += fs::append(file, bufs);
		++num;

		char pbuf[48];
		log::info
		{
			log, "dump[%s] rooms:%zu %s %s",
			filename,
			num,
			pretty(pbuf, iec(len)),
			string_view{room_id},
		};

		return true;
	});

	char pbuf[48];
	log::notice
	{
		log, "dump[%s] complete rooms:%zu using %s",
		filename,
		num,
		pretty(pbuf, iec(len)),
    };
}

bool
ircd::m::rooms::has(const opts &opts)
{
	return !for_each(opts, []
	(const m::room::id &)
	{
		// false to break; for_each() returns false
		return false;
	});
}

size_t
ircd::m::rooms::count(const opts &opts)
{
	size_t ret{0};
	for_each(opts, [&ret]
	(const m::room::id &)
	{
		++ret;
		return true;
	});

	return ret;
}

bool
ircd::m::rooms::for_each(const room::id::closure_bool &closure)
{
	return for_each(opts_default, closure);
}

bool
ircd::m::rooms::for_each(const opts &opts,
                         const room::id::closure_bool &closure)
{
	bool ret{true};
	const auto proffer{[&opts, &closure, &ret]
	(const m::room::id &room_id)
	{
		if(opts.room_id && !opts.lower_bound)
		{
			ret = false;
			return;
		}

		const m::room room
		{
			room_id
		};

		if(opts.room_id)
			if(room_id < opts.room_id)
				return;

		if(opts.local_joined_only)
			if(!local_joined(room))
				return;

		if(opts.remote_joined_only)
			if(!remote_joined(room))
				return;

		if(opts.local_only)
			if(!local_only(room))
				return;

		if(opts.remote_only)
			if(local_only(room))
				return;

		if(opts.server && !opts.summary)
			if(opts.server != room_id.host())
				return;

		if(opts.join_rule && !opts.summary)
			if(!join_rule(room, opts.join_rule))
				return;

		if(opts.room_alias)
		{
			const auto match_alias_prefix{[&opts](const auto &alias)
			{
				return !startswith(alias, opts.room_alias);
			}};

			if(room::aliases(room).for_each(match_alias_prefix))
				return; // no match
		}

		ret = closure(room);
	}};

	// branch for public rooms of a specific user
	if(opts.user_id)
	{
		const user::rooms user_rooms
		{
			opts.user_id
		};

		return user_rooms.for_each(user::rooms::closure_bool{[&proffer, &ret]
		(const room::id &room_id, const string_view &membership)
		{
			proffer(room_id);
			return ret;
		}});
	}

	// branch for optimized public rooms searches.
	if(opts.summary)
	{
		const room::id::buf public_room_id
		{
			"!public", my_host()
		};

		const room::state state
		{
			public_room_id
		};

		state.for_each("ircd.rooms.summary", [&opts, &proffer, &ret]
		(const string_view &type, const string_view &state_key, const event::idx &event_idx)
		{
			const auto &[room_id, origin]
			{
				rooms::summary::unmake_state_key(state_key)
			};

			if(opts.server && origin != opts.server)
				return true;

			proffer(room_id);
			return ret;
		});

		return ret;
	}

	ctx::dock dock;
	size_t fetch {0}, fetched {0};
	const auto fetcher{[&]
	(const string_view &type, const event::idx &event_idx)
	{
		++fetch;
		fetched += m::get(std::nothrow, event_idx, "room_id", proffer);
		dock.notify_one();
		return ret;
	}};

	size_t prefetch {0}, prefetched {0};
	const auto prefetcher{[&]
	(const string_view &type, const event::idx &event_idx)
	{
		++prefetch;
		prefetched += m::prefetch(event_idx, "room_id");
		dock.wait([&]
		{
			return fetch + opts.prefetch > prefetch;
		});

		return ret;
	}};

	if(!opts.prefetch)
		return events::type::for_each_in("m.room.create", fetcher);

	const ctx::context prefetch_worker
	{
		"m.rooms.prefetch", [&prefetcher]
		{
			events::type::for_each_in("m.room.create", prefetcher);
		}
	};

	return events::type::for_each_in("m.room.create", fetcher);
}

//
// ircd::m::rooms::opts::opts
//

ircd::m::rooms::opts::opts(const string_view &input)
noexcept
:room_id
{
	valid(m::id::ROOM, input)?
		m::id::room{input}:
		m::id::room{}
}
,server
{
	startswith(input, ':')?
		lstrip(input, ':'):
		string_view{}
}
,room_alias
{
	valid(m::id::ROOM_ALIAS, input)?
		m::id::room_alias{input}:
		m::id::room_alias{}
}
,user_id
{
	valid(m::id::USER, input)?
		m::id::user{input}:
		m::id::user{}
}
,local_only
{
	has(input, "local_only")
}
,remote_only
{
	has(input, "remote_only")
}
,local_joined_only
{
	has(input, "local_joined_only")
}
,remote_joined_only
{
	has(input, "remote_joined_only")
}
,search_term
{
	!room_id && !server && !room_alias && !user_id
	&& !local_only && !remote_only
	&& !local_joined_only && !remote_joined_only?
		input:
		string_view{}
}
{
}
