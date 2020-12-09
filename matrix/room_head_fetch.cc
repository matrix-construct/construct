// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

decltype(ircd::m::room::head::fetch::timeout)
ircd::m::room::head::fetch::timeout
{
	{ "name",     "ircd.m.room.head.fetch.timeout" },
	{ "default",  10 * 1000L                       },
};

ircd::m::event::id::buf
ircd::m::room::head::fetch::one(const id &room_id,
                                const string_view &remote,
                                const user::id &user_id)
{
	const unique_mutable_buffer buf
	{
		16_KiB
	};

	const m::event event
	{
		one(buf, room_id, remote, user_id)
	};

	const m::event::prev prev
	{
		event
	};

	return prev.prev_event(0);
}

ircd::m::event
ircd::m::room::head::fetch::one(const mutable_buffer &out,
                                const id &room_id,
                                const string_view &remote,
                                const user::id &user_id_)
{
	const m::room room
	{
		room_id
	};

	// When no user_id is supplied and the room exists locally we attempt
	// to find the user_id of one of our users with membership in the room.
	// This satisfies synapse's requirements for whether we have access
	// to the response. If user_id remains blank then make_join will later
	// generate a random one from our host as well.
	m::user::id::buf user_id
	{
		!user_id_?
			any_user(room, my_host(), "join"):
			user_id_
	};

	// Make another attempt to find an invited user because that carries some
	// value (this query is not as fast as querying join memberships).
	if(!user_id)
		user_id = any_user(room, my_host(), "invite");

	const bool internal_alloc
	{
		size(out) < 16_KiB
	};

	const unique_mutable_buffer internal_buf
	{
		internal_alloc? 16_KiB: 0_KiB
	};

	const mutable_buffer buf
	{
		internal_alloc? internal_buf: out
	};

	assert(size(buf) >= 16_KiB);
	fed::make_join::opts opts;
	opts.remote = remote;
	opts.dynamic = false;
	fed::make_join request
	{
		room_id, user_id, buf, std::move(opts)
	};

	const auto code
	{
		request.get(milliseconds(timeout))
	};

	const json::object proto
	{
		request.in.content
	};

	const json::object event
	{
		proto["event"]
	};

	const size_t moved
	{
		move(out, string_view(event))
	};

	assert(moved == size(string_view(event)));
	return json::object
	{
		string_view
		{
			data(out), moved
		}
	};
}

//
// fetch::fetch
//

ircd::m::room::head::fetch::fetch(const opts &opts,
                                  const closure &closure)
{
	const m::room room
	{
		opts.room_id
	};

	// When the room isn't public we need to supply a user_id of one of our
	// users in the room to satisfy matrix protocol requirements upstack.
	const m::user::id::buf user_id
	{
		!opts.user_id?
			m::any_user(room, origin(my()), "join"):
			opts.user_id
	};

	std::tuple<id::event::buf, int64_t, event::idx> top;
	std::get<2>(top) = std::get<2>(opts.top);
	std::get<1>(top) = std::get<1>(opts.top);
	if(std::get<0>(opts.top))
		std::get<0>(top) = std::get<0>(opts.top);

	time_t top_ots {0};
	auto &[top_event_id, top_depth, top_idx]
	{
		top
	};

	if(!top_event_id && !top_idx)
		top = m::top(std::nothrow, room);

	if(!top_event_id)
		top_event_id = m::head(std::nothrow, room);

	if(!top_idx)
		top_idx = m::index(std::nothrow, top_event_id);

	if(!top_depth)
		m::get(std::nothrow, top_idx, "depth", top_depth);

	if(!top_ots)
		m::get(std::nothrow, top_idx, "origin_server_ts", top_ots);

	char tmbuf[48];
	log::debug
	{
		log, "Resynchronizing %s from %s [relative idx:%lu depth:%ld %s] from %zu joined servers...",
		string_view{room.room_id},
		string_view{top_event_id},
		top_idx,
		top_depth,
		microdate(tmbuf),
		room::origins(room).count(),
	};

	m::event result;
	if(likely(closure))
		json::get<"room_id"_>(result) = opts.room_id;

	feds::opts fopts;
	fopts.op = feds::op::head;
	fopts.room_id = room.room_id;
	fopts.user_id = user_id;
	fopts.closure_errors = false; // exceptions wil not propagate feds::execute
	fopts.exclude_myself = true;
	fopts.timeout = milliseconds(timeout);
	feds::execute(fopts, [this, &opts, &top, &top_ots, &closure, &result]
	(const auto &response)
	{
		m::event event
		{
			response.object.get("event")
		};

		const event::prev prev
		{
			event
		};

		const auto heads
		{
			prev.prev_events_count()
		};

		// The depth comes back as one greater than any existing
		// depth so we subtract one.
		const auto &depth
		{
			std::max(json::get<"depth"_>(event) - 1L, 0L)
		};

		const auto &ots
		{
			json::get<"origin_server_ts"_>(event)
		};

		const auto &[top_event_id, top_depth, top_idx]
		{
			top
		};

		this->respond += 1;
		this->heads += heads;
		this->ots[0] += ots < top_ots;
		this->ots[1] += ots == top_ots;
		this->ots[2] += ots > top_ots;
		this->depth[0] += depth < top_depth;
		this->depth[1] += depth == top_depth;
		this->depth[2] += depth > top_depth;

		if(likely(closure))
		{
			json::get<"origin"_>(result) = response.origin;
			json::get<"origin_server_ts"_>(result) = ots;
			json::get<"depth"_>(result) = depth;
		}

		size_t i(0);
		return m::for_each(prev, [this, &opts, &closure, &result, &i]
		(const event::id &event_id)
		{
			if(unlikely(i++ > opts.max_results_per_server))
				return true;

			if(unlikely(this->head.size() >= opts.max_results))
				return false;

			auto it
			{
				this->head.lower_bound(event_id)
			};

			if(likely(opts.unique))
				if(it != std::end(this->head) && *it == event_id)
				{
					++this->concur;
					return true;
				}

			if(likely(!opts.existing))
				if(m::exists(event_id))
				{
					++this->exists;
					return true;
				}

			if(likely(opts.unique))
			{
				it = this->head.emplace_hint(it, event_id);
				result.event_id = *it;
			}
			else result.event_id = event_id;

			if(likely(closure))
				return closure(result);

			return true;
		});
	});
}
