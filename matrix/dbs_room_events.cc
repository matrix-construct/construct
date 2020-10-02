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
	static bool room_events__cmp_lt(const string_view &, const string_view &);
}

/// Linkage for a reference to the room_events column
decltype(ircd::m::dbs::room_events)
ircd::m::dbs::room_events;

decltype(ircd::m::dbs::desc::room_events__comp)
ircd::m::dbs::desc::room_events__comp
{
	{ "name",     "ircd.m.dbs._room_events.comp" },
	{ "default",  "default"                      },
};

decltype(ircd::m::dbs::desc::room_events__block__size)
ircd::m::dbs::desc::room_events__block__size
{
	{ "name",     "ircd.m.dbs._room_events.block.size" },
	{ "default",  512L                                 },
};

decltype(ircd::m::dbs::desc::room_events__meta_block__size)
ircd::m::dbs::desc::room_events__meta_block__size
{
	{ "name",     "ircd.m.dbs._room_events.meta_block.size" },
	{ "default",  long(16_KiB)                              },
};

decltype(ircd::m::dbs::desc::room_events__cache__size)
ircd::m::dbs::desc::room_events__cache__size
{
	{
		{ "name",     "ircd.m.dbs._room_events.cache.size" },
		{ "default",  long(32_MiB)                         },
	}, []
	{
		const size_t &value{room_events__cache__size};
		db::capacity(db::cache(dbs::room_events), value);
	}
};

decltype(ircd::m::dbs::desc::room_events__cache_comp__size)
ircd::m::dbs::desc::room_events__cache_comp__size
{
	{
		{ "name",     "ircd.m.dbs._room_events.cache_comp.size" },
		{ "default",  long(16_MiB)                              },
	}, []
	{
		const size_t &value{room_events__cache_comp__size};
		db::capacity(db::cache_compressed(dbs::room_events), value);
	}
};

/// Prefix transform for the room_events. The prefix here is a room_id
/// and the suffix is the depth+event_id concatenation.
/// for efficient sequences
///
const ircd::db::prefix_transform
ircd::m::dbs::desc::room_events__pfx
{
	"_room_events",

	[](const string_view &key)
	{
		return has(key, "\0"_sv);
	},

	[](const string_view &key)
	{
		return split(key, '\0').first;
	}
};

/// Comparator for the room_events. The goal here is to sort the
/// events within a room by their depth from highest to lowest, so the
/// highest depth is hit first when a room is sought from this column.
///
const ircd::db::comparator
ircd::m::dbs::desc::room_events__cmp
{
	"_room_events",
	room_events__cmp_lt,
	db::cmp_string_view::equal,
};

/// This column stores events in sequence in a room. Consider the following:
///
/// [room_id | depth + event_idx]
///
/// The key is composed from three parts:
///
/// - `room_id` is the official prefix, bounding the sequence. That means we
/// make a blind query with just a room_id and get to the beginning of the
/// sequence, then iterate until we stop before the next room_id (upper bound).
///
/// - `depth` is the ordering. Within the sequence, all elements are ordered by
/// depth from HIGHEST TO LOWEST. The sequence will start at the highest depth.
/// NOTE: Depth is a fixed 8 byte binary integer.
///
/// - `event_idx` is the key suffix. This column serves to sequence all events
/// within a room ordered by depth. There may be duplicate room_id|depth
/// prefixing but the event_idx suffix gives the key total uniqueness.
/// NOTE: event_idx is a fixed 8 byte binary integer.
///
const ircd::db::descriptor
ircd::m::dbs::desc::room_events
{
	// name
	"_room_events",

	// explanation
	R"(Indexes events in timeline sequence for a room

	[room_id | depth + event_idx]

	)",

	// typing (key, value)
	{
		typeid(string_view), typeid(string_view)
	},

	// options
	{},

	// comparator
	room_events__cmp,

	// prefix transform
	room_events__pfx,

	// drop column
	false,

	// cache size
	bool(cache_enable)? -1 : 0,

	// cache size for compressed assets
	bool(cache_comp_enable)? -1 : 0,

	// bloom filter bits
	0, // no bloom filter because of possible comparator issues

	// expect queries hit
	true,

	// block size
	size_t(room_events__block__size),

	// meta_block size
	size_t(room_events__meta_block__size),

	// compression
	string_view{room_events__comp},
};

//
// indexer
//

/// Adds the entry for the room_events column into the txn.
void
ircd::m::dbs::_index_room_events(db::txn &txn,
                                 const event &event,
                                 const write_opts &opts)
{
	assert(opts.appendix.test(appendix::ROOM_EVENTS));

	thread_local char buf[ROOM_EVENTS_KEY_MAX_SIZE];
	const ctx::critical_assertion ca;
	const string_view &key
	{
		room_events_key(buf, at<"room_id"_>(event), at<"depth"_>(event), opts.event_idx)
	};

	db::txn::append
	{
		txn, room_events,
		{
			opts.op,        // db::op
			key,            // key,
		}
	};
}

//
// cmp
//

bool
ircd::m::dbs::room_events__cmp_lt(const string_view &a,
                                  const string_view &b)
{
	static const auto &pt
	{
		desc::room_events__pfx
	};

	// Extract the prefix from the keys
	const string_view pre[2]
	{
		pt.get(a),
		pt.get(b),
	};

	if(size(pre[0]) != size(pre[1]))
		return size(pre[0]) < size(pre[1]);

	if(pre[0] != pre[1])
		return pre[0] < pre[1];

	// After the prefix is the depth + event_idx
	const string_view post[2]
	{
		a.substr(size(pre[0])),
		b.substr(size(pre[1])),
	};

	// These conditions are matched on some queries when the user only
	// supplies a room id.

	if(empty(post[0]))
		return true;

	if(empty(post[1]))
		return false;

	// Distill out the depth and event_idx integers

	const auto &[depth_a, event_idx_a]
	{
		room_events_key(post[0])
	};

	const auto &[depth_b, event_idx_b]
	{
		room_events_key(post[1])
	};

	// When two events are at the same depth sort by index (the sequence
	// number given as they were admitted into the system) otherwise
	// sort by depth. Note this is a reverse order comparison.
	const auto ret
	{
		depth_b != depth_a?
			depth_b < depth_a:
			event_idx_b < event_idx_a
	};

	return ret;
}

//
// key
//

std::tuple<uint64_t, ircd::m::event::idx>
ircd::m::dbs::room_events_key(const string_view &amalgam)
{
	assert(size(amalgam) >= 1 + 8 + 8 || size(amalgam) == 1 + 8);
	assert(amalgam.front() == '\0');

	const uint64_t &depth
	{
		*reinterpret_cast<const uint64_t *>(data(amalgam) + 1)
	};

	const event::idx &event_idx
	{
		size(amalgam) >= 1 + 8 + 8?
			*reinterpret_cast<const event::idx *>(data(amalgam) + 1 + 8):
			std::numeric_limits<event::idx>::max()
	};

	// Make sure these are copied rather than ever returning references in
	// a tuple because the chance the integers will be aligned is low.
	return { depth, event_idx };
}

ircd::string_view
ircd::m::dbs::room_events_key(const mutable_buffer &out_,
                              const id::room &room_id,
                              const uint64_t &depth)
{
	mutable_buffer out{out_};
	consume(out, copy(out, room_id));
	consume(out, copy(out, '\0'));
	consume(out, copy(out, byte_view<string_view>(depth)));
	const mutable_buffer ret
	{
		data(out_), data(out)
	};

	assert(size(ret) == size(room_id) + 1 + 8);
	return ret;
}

ircd::string_view
ircd::m::dbs::room_events_key(const mutable_buffer &out_,
                              const id::room &room_id,
                              const uint64_t &depth,
                              const event::idx &event_idx)
{
	mutable_buffer out{out_};
	consume(out, copy(out, room_id));
	consume(out, copy(out, '\0'));
	consume(out, copy(out, byte_view<string_view>(depth)));
	consume(out, copy(out, byte_view<string_view>(event_idx)));
	const mutable_buffer ret
	{
		data(out_), data(out)
	};

	assert(size(ret) == size(room_id) + 1 + 8 + 8);
	return ret;
}
