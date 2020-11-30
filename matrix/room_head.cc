// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m
{
	static void append_v1(json::stack::array &, const event::id &);
	static void append_v3(json::stack::array &, const event::id &);
}

ircd::m::room::head::generate::generate(const mutable_buffer &buf,
                                        const m::room::head &head,
                                        const opts &opts)
{
	if(empty(buf))
		return;

	json::stack out{buf};
	{
		json::stack::array array
		{
			out
		};

		const generate g
		{
			array, head, opts
		};

		this->depth = g.depth;
	}

	this->array = out.completed();
}

ircd::m::room::head::generate::generate(json::stack::array &out,
                                        const m::room::head &head,
                                        const opts &opts)
try
{
	if(!head.room)
		return;

	// Query the room version unless hinted in the opts
	char versionbuf[32];
	const auto version
	{
		opts.version?:
			m::version(versionbuf, head.room, std::nothrow)
	};

	// The output format depends on the room version; we select an output
	// function for the format here so we can abstractly call append().
	const auto &append
	{
		version == "1" || version == "2"?
			append_v1:
			append_v3
	};

	// When the top_head option is given we query for that here
	std::tuple<m::id::event::buf, int64_t, m::event::idx> top_head;
	if(opts.need_top_head)
		top_head = m::top(std::nothrow, head.room.room_id);

	// Iterate the room head; starts with oldest events
	bool need_top_head{opts.need_top_head};
	bool need_my_head{opts.need_my_head};
	ssize_t limit(opts.limit);
	head.for_each([&](const event::idx &event_idx, const event::id &event_id)
	{
		// Determine the depth for metrics
		const int64_t depth
		{
			event_id == std::get<0>(top_head)?
				std::get<int64_t>(top_head):
				m::get<int64_t>(std::nothrow, event_idx, "depth", -1L)
		};

		if(unlikely(depth < 0))
		{
			log::derror
			{
				log, "Missing depth for %s idx:%lu in room head of %s",
				string_view{event_id},
				event_idx,
				string_view{head.room.room_id},
			};

			return true;
		}

		// When using the need_my_head option, if we hit a head which
		// originated from this server we mark that is no longer needed.
		if(need_my_head && event::my(event_idx))
			need_my_head = false;

		// If we hit the top_head during the loop we can mark that satisfied.
		if(need_top_head && event_id == std::get<0>(top_head))
			need_top_head = false;

		// Two reference slots are reserved to fulfill these features; the
		// loop will iterate without appending anything else.
		const ssize_t remain
		{
			limit - need_my_head - need_top_head
		};

		// Skip/continue the loop if all that remains are reserved slots.
		if(remain <= 0)
			return true;

		// Add this head reference to result to output.
		append(out, event_id);

		// Indicate if this depth is highest or lowest of the set.
		this->depth[0] = std::min(depth, this->depth[0]);
		this->depth[1] = std::max(depth, this->depth[1]);

		// Continue loop until we're out of slots.
		return --limit > 0;
	});

	// If the iteration did not provide us with the top_head and the opts
	// require it, we add that here.
	if(need_top_head && std::get<0>(top_head))
	{
		assert(limit > 0);
		append(out, std::get<0>(top_head));
		this->depth[1] = std::get<1>(top_head);
		need_top_head = false;
		--limit;
		if(need_my_head && event::my(std::get<2>(top_head)))
			need_my_head = false;
	}

	// If the iteration did not provide us with any heads from this origin
	// and the opts require it, we find and add that here. Also if no heads
	// whatsoever have been found this branch is also taken.
	if(need_my_head || size_t(limit) == opts.limit)
		for(m::room::events it{head.room}; it; --it)
		{
			if(need_my_head && !event::my(it.event_idx()))
				continue;

			const auto event_id
			{
				m::event_id(std::nothrow, it.event_idx())
			};

			if(unlikely(!event_id))
				continue;

			assert(limit > 0);
			append(out, event_id);
			const int64_t &depth(it.depth());
			this->depth[0] = std::min(depth, this->depth[0]);
			this->depth[1] = std::max(depth, this->depth[1]);
			need_my_head = false;
			--limit;
			break;
		}

	assert(limit >= 0);
	if(unlikely(opts.limit && size_t(limit) == opts.limit))
		throw error
		{
			"Failed to find any events at the room head"
		};
}
catch(const std::exception &e)
{
	log::error
	{
		log, "%s prev_events generator :%s",
		string_view{head.room},
		e.what(),
	};

	throw;
}

void
ircd::m::append_v1(json::stack::array &out,
                   const event::id &event_id)
{
	json::stack::array prev
	{
		out
	};

	// [0]
	assert(event_id);
	prev.append(event_id);

	// [1]
	json::stack::object nilly
	{
		prev
	};

	json::stack::member
	{
		nilly, "", ""
	};
}

void
ircd::m::append_v3(json::stack::array &out,
                   const event::id &event_id)
{
	assert(event_id);
	out.append(event_id);
}

size_t
ircd::m::room::head::count()
const
{
	size_t ret(0);
	for_each([&ret]
	(const event::idx &event_idx, const event::id &event_id)
	{
		++ret;
		return true;
	});

	return ret;
}

bool
ircd::m::room::head::has(const event::id &event_id)
const
{
	bool ret{false};
	for_each([&ret, &event_id]
	(const event::idx &event_idx, const event::id &event_id_)
	{
		ret = event_id_ == event_id;
		return !ret; // for_each protocol: false to break
	});

	return ret;
}

bool
ircd::m::room::head::for_each(const closure &closure)
const
{
	auto it
	{
		dbs::room_head.begin(room.room_id)
	};

	for(; it; ++it)
	{
		const event::id &event_id
		{
			dbs::room_head_key(it->first)
		};

		const event::idx &event_idx
		{
			byte_view<event::idx>{it->second}
		};

		if(!closure(event_idx, event_id))
			return false;
	}

	return true;
}

//
// special tools
//

size_t
ircd::m::room::head::reset(const head &head)
{
	size_t ret{0};
	const auto &room{head.room};
	m::room::events it{room};
	if(!it)
		return ret;

	// Replacement will be the single new head
	const m::event replacement
	{
		*it
	};

	db::txn txn
	{
		*m::dbs::events
	};

	// Iterate all of the existing heads with a delete operation
	m::dbs::write_opts opts;
	opts.op = db::op::DELETE;
	opts.appendix.reset();
	opts.appendix.set(dbs::appendix::ROOM_HEAD);
	head.for_each([&room, &opts, &txn, &ret]
	(const m::event::idx &event_idx, const m::event::id &event_id)
	{
		const m::event::fetch event
		{
			std::nothrow, event_idx
		};

		if(!event.valid)
		{
			log::derror
			{
				m::log, "Invalid event '%s' idx %lu in head for %s",
				string_view{event_id},
				event_idx,
				string_view{room.room_id}
			};

			return true;
		}

		opts.event_idx = event_idx;
		m::dbs::write(txn, event, opts);
		++ret;
		return true;
	});

	// Finally add the replacement to the txn
	opts.op = db::op::SET;
	opts.event_idx = it.event_idx();
	m::dbs::write(txn, replacement, opts);

	// Commit txn
	txn();
	return ret;
}

size_t
ircd::m::room::head::rebuild(const head &head)
{
	size_t ret{0};
	static const m::event::fetch::opts fopts
	{
		{ db::get::NO_CACHE }
	};

	m::room::events it
	{
		head.room, 0UL, &fopts
	};

	if(!it)
		return ret;

	db::txn txn
	{
		*m::dbs::events
	};

	m::dbs::write_opts opts;
	opts.op = db::op::SET;
	for(; it; ++it)
	{
		const m::event &event{*it};
		opts.event_idx = it.event_idx();
		opts.appendix.reset();
		opts.appendix.set(dbs::appendix::ROOM_HEAD);
		m::dbs::write(txn, event, opts);
		++ret;
	}

	txn();
	return ret;
}

void
ircd::m::room::head::modify(const m::event::id &event_id,
                            const db::op &op,
                            const bool &refs)
{
	const m::event::fetch event
	{
		event_id
	};

	db::txn txn
	{
		*m::dbs::events
	};

	// Iterate all of the existing heads with a delete operation
	m::dbs::write_opts opts;
	opts.op = op;
	opts.event_idx = event.event_idx;
	opts.appendix.reset();
	opts.appendix.set(dbs::appendix::ROOM_HEAD);
	m::dbs::write(txn, event, opts);

	// Commit txn
	txn();
}
