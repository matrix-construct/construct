// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m::dbs
{
	static bool event_state__cmp_lt(const string_view &, const string_view &);
}

decltype(ircd::m::dbs::event_state)
ircd::m::dbs::event_state;

decltype(ircd::m::dbs::desc::event_state__comp)
ircd::m::dbs::desc::event_state__comp
{
	{ "name",     "ircd.m.dbs._event_state.comp" },
	{ "default",  "default"                      },
};

decltype(ircd::m::dbs::desc::event_state__block__size)
ircd::m::dbs::desc::event_state__block__size
{
	{ "name",     "ircd.m.dbs._event_state.block.size" },
	{ "default",  512L                                 },
};

decltype(ircd::m::dbs::desc::event_state__meta_block__size)
ircd::m::dbs::desc::event_state__meta_block__size
{
	{ "name",     "ircd.m.dbs._event_state.meta_block.size" },
	{ "default",  long(2_KiB)                               },
};

decltype(ircd::m::dbs::desc::event_state__cache__size)
ircd::m::dbs::desc::event_state__cache__size
{
	{
		{ "name",     "ircd.m.dbs._event_state.cache.size" },
		{ "default",  long(32_MiB)                         },
	}, []
	{
		const size_t &value{event_state__cache__size};
		db::capacity(db::cache(dbs::event_state), value);
	}
};

decltype(ircd::m::dbs::desc::event_state__cache_comp__size)
ircd::m::dbs::desc::event_state__cache_comp__size
{
	{
		{ "name",     "ircd.m.dbs._event_state.cache_comp.size" },
		{ "default",  long(0_MiB)                               },
	}, []
	{
		const size_t &value{event_state__cache_comp__size};
		db::capacity(db::cache_compressed(dbs::event_state), value);
	}
};

const ircd::db::comparator
ircd::m::dbs::desc::event_state__cmp
{
	"_event_state",
	event_state__cmp_lt,
	db::cmp_string_view::equal,
};

const ircd::db::descriptor
ircd::m::dbs::desc::event_state
{
	// name
	"_event_state",

	// explanation
	R"(Index of states of events.

	state_key, type, room_id, depth, event_idx => --

	The state transitions of events are indexed by this column,
	based on the state_key property.

	)",

	// typing (key, value)
	{
		typeid(string_view), typeid(string_view)
	},

	// options
	{},

	// comparator
	event_state__cmp,

	// prefix transform
	{},

	// drop column
	false,

	// cache size
	bool(cache_enable)? -1 : 0, //uses conf item

	// cache size for compressed assets
	bool(cache_comp_enable)? -1 : 0,

	// bloom filter bits
	0,

	// expect queries hit
	false,

	// block size
	size_t(event_state__block__size),

	// meta_block size
	size_t(event_state__meta_block__size),

	// compression
	string_view{event_state__comp},

	// compactor
	{},

	// compaction priority algorithm
	"kOldestSmallestSeqFirst"s,
};

//
// indexer
//

void
ircd::m::dbs::_index_event_state(db::txn &txn,
                                 const event &event,
                                 const write_opts &opts)
{
	assert(opts.appendix.test(appendix::EVENT_STATE));
	assert(json::get<"type"_>(event));
	assert(opts.event_idx);

	if(!defined(json::get<"state_key"_>(event)))
		return;

	thread_local char buf[EVENT_STATE_KEY_MAX_SIZE];
	db::txn::append
	{
		txn, dbs::event_state,
		{
			opts.op, event_state_key(buf, event_state_tuple
			{
				at<"state_key"_>(event),
				at<"type"_>(event),
				at<"room_id"_>(event),
				at<"depth"_>(event),
				opts.event_idx,
			})
		}
	};
}

//
// cmp
//

bool
ircd::m::dbs::event_state__cmp_lt(const string_view &a,
                                  const string_view &b)
{
	const auto &[state_key_a, type_a, room_id_a, depth_a, event_idx_a]
	{
		event_state_key(a)
	};

	const auto &[state_key_b, type_b, room_id_b, depth_b, event_idx_b]
	{
		event_state_key(b)
	};

	if(state_key_a != state_key_b)
		return state_key_a < state_key_b;

	if(type_a != type_b)
		return type_a < type_b;

	if(room_id_a != room_id_b)
		return room_id_a < room_id_b;

	if(depth_a != depth_b)
		return depth_a > depth_b;

	if(event_idx_a != event_idx_b)
		return event_idx_a > event_idx_b;

	return false;
}

//
// key
//

ircd::m::dbs::event_state_tuple
ircd::m::dbs::event_state_key(const string_view &amalgam)
{
	assert(!startswith(amalgam, '\0'));
	const auto &[state_key, r0]
	{
		split(amalgam, '\0')
	};

	const auto &[type, r1]
	{
		split(r0, '\0')
	};

	const auto &[room_id, r2]
	{
		split(r1, '\0')
	};

	assert(!room_id || m::valid(m::id::ROOM, room_id));
	return event_state_tuple
	{
		state_key,
		type,
		room_id,
		r2.size() >= 8?
			int64_t(byte_view<int64_t>(r2.substr(0, 8))):
			-1L,
		r2.size() >= 16?
			event::idx(byte_view<uint64_t>(r2.substr(8))):
			0UL,
	};
}

ircd::string_view
ircd::m::dbs::event_state_key(const mutable_buffer &out_,
                              const event_state_tuple &tuple)
{
	assert(size(out_) >= EVENT_STATE_KEY_MAX_SIZE);

	const auto &[state_key, type, room_id, depth, event_idx]
	{
		tuple
	};

	if(!state_key)
		return {};

	mutable_buffer out{out_};
	consume(out, copy(out, trunc(state_key, event::STATE_KEY_MAX_SIZE)));

	if(!type)
		return {data(out_), data(out)};

	consume(out, copy(out, '\0'));
	consume(out, copy(out, trunc(type, event::TYPE_MAX_SIZE)));

	if(!room_id)
		return {data(out_), data(out)};

	assert(m::valid(m::id::ROOM, room_id));
	consume(out, copy(out, '\0'));
	consume(out, copy(out, room_id));

	if(likely(depth < 0))
		return {data(out_), data(out)};

	consume(out, copy(out, '\0'));
	consume(out, copy(out, byte_view<string_view>(depth)));

	if(likely(!event_idx))
		return {data(out_), data(out)};

	consume(out, copy(out, byte_view<string_view>(event_idx)));
	return {data(out_), data(out)};
}
