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
	static bool room_type__cmp_lt(const string_view &, const string_view &);
}

decltype(ircd::m::dbs::room_type)
ircd::m::dbs::room_type;

decltype(ircd::m::dbs::desc::room_type__comp)
ircd::m::dbs::desc::room_type__comp
{
	{ "name",     "ircd.m.dbs._room_type.comp" },
	{ "default",  "default"                    },
};

decltype(ircd::m::dbs::desc::room_type__block__size)
ircd::m::dbs::desc::room_type__block__size
{
	{ "name",     "ircd.m.dbs._room_type.block.size" },
	{ "default",  512L                               },
};

decltype(ircd::m::dbs::desc::room_type__meta_block__size)
ircd::m::dbs::desc::room_type__meta_block__size
{
	{ "name",     "ircd.m.dbs._room_type.meta_block.size" },
	{ "default",  8192L                                   },
};

decltype(ircd::m::dbs::desc::room_type__cache__size)
ircd::m::dbs::desc::room_type__cache__size
{
	{
		{ "name",     "ircd.m.dbs._room_type.cache.size" },
		{ "default",  long(16_MiB)                       },
	}, []
	{
		const size_t &value{room_type__cache__size};
		db::capacity(db::cache(dbs::room_type), value);
	}
};

decltype(ircd::m::dbs::desc::room_type__cache_comp__size)
ircd::m::dbs::desc::room_type__cache_comp__size
{
	{
		{ "name",     "ircd.m.dbs._room_type.cache_comp.size" },
		{ "default",  long(8_MiB)                             },
	}, []
	{
		const size_t &value{room_type__cache_comp__size};
		db::capacity(db::cache_compressed(dbs::room_type), value);
	}
};

/// Prefix transform for the room_type. The prefix here is a room_id
/// and the suffix is the type+depth+event_id concatenation.
/// for efficient sequences
///
const ircd::db::prefix_transform
ircd::m::dbs::desc::room_type__pfx
{
	"_room_type",

	[](const string_view &key)
	{
		return has(key, "\0"_sv);
	},

	[](const string_view &key)
	{
		return split(key, '\0').first;
	}
};

/// Comparator for the room_type. The goal here is to sort the
/// events within a room by their depth from highest to lowest, so the
/// highest depth is hit first when a room is sought from this column.
///
const ircd::db::comparator
ircd::m::dbs::desc::room_type__cmp
{
	"_room_type",
	room_type__cmp_lt,
	db::cmp_string_view::equal,
};

/// This column stores events by type in sequence in a room. Consider the
/// following:
///
/// [room_id | type, depth, event_idx]
///
const ircd::db::descriptor
ircd::m::dbs::desc::room_type
{
	// name
	"_room_type",

	// explanation
	R"(Indexes events per type in timeline sequence for a room

	[room_id | type, depth, event_idx]

	)",

	// typing (key, value)
	{
		typeid(string_view), typeid(string_view)
	},

	// options
	{},

	// comparator
	room_type__cmp,

	// prefix transform
	room_type__pfx,

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
	size_t(room_type__block__size),

	// meta_block size
	size_t(room_type__meta_block__size),

	// compression
	string_view{room_type__comp},

	// compactor
	{},

	// compaction priority algorithm
	"kOldestSmallestSeqFirst"s,
};

//
// indexer
//

/// Adds the entry for the room_type column into the txn.
void
ircd::m::dbs::_index_room_type(db::txn &txn,
                               const event &event,
                               const write_opts &opts)
{
	assert(opts.appendix.test(appendix::ROOM_TYPE));

	thread_local char buf[ROOM_TYPE_KEY_MAX_SIZE];
	const ctx::critical_assertion ca;
	const string_view &key
	{
		room_type_key(buf, at<"room_id"_>(event), at<"type"_>(event), at<"depth"_>(event), opts.event_idx)
	};

	db::txn::append
	{
		txn, room_type,
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
ircd::m::dbs::room_type__cmp_lt(const string_view &a,
                                const string_view &b)
{
	static const auto &pt
	{
		desc::room_type__pfx
	};

	// Extract the prefix from the keys
	const string_view pre[2]
	{
		pt.get(a),
		pt.get(b),
	};

	// Prefix size comparison has highest priority for rocksdb
	if(size(pre[0]) < size(pre[1]))
		return true;

	// Prefix size comparison has highest priority for rocksdb
	if(size(pre[0]) > size(pre[1]))
		return false;

	// Prefix lexical comparison sorts prefixes of the same size
	if(pre[0] < pre[1])
		return true;

	// Prefix lexical comparison sorts prefixes of the same size
	if(pre[0] > pre[1])
		return false;

	// After the prefix is the \0,type,\0,depth,event_idx
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

	const auto &[type_a, depth_a, event_idx_a]
	{
		room_type_key(post[0])
	};

	const auto &[type_b, depth_b, event_idx_b]
	{
		room_type_key(post[1])
	};

	if(type_a < type_b)
		return true;

	if(type_a > type_b)
		return false;

	// reverse depth to start from highest first like room_events
	if(depth_a < depth_b)
		return false;

	// reverse depth to start from highest first like room_events
	if(depth_a > depth_b)
		return true;

	// reverse event_idx to start from highest first like room_events)
	if(event_idx_a < event_idx_b)
		return false;

	if(event_idx_a > event_idx_b)
		return true;

	// equal is not less; so false
	return false;
}

//
// key
//

ircd::m::dbs::room_type_tuple
ircd::m::dbs::room_type_key(const string_view &amalgam_)
{
	assert(size(amalgam_) >= 1 + 1 + 8 + 8);

	assert(amalgam_.front() == '\0');
	const string_view &amalgam
	{
		amalgam_.substr(1)
	};

	assert(amalgam.size() >= 1 + 8 + 1);
	const auto &[type, trail]
	{
		split(amalgam, '\0')
	};

	assert(trail.size() >= 8 + 8);
	return room_type_tuple
	{
		type,
		likely(trail.size() >= 8)?
			uint64_t(byte_view<uint64_t>(trail.substr(0, 8))):
			-1UL,
		likely(trail.size() >= 16)?
			event::idx(byte_view<uint64_t>(trail.substr(8))):
			0UL,
	};
}

ircd::string_view
ircd::m::dbs::room_type_key(const mutable_buffer &out_,
                            const id::room &room_id,
                            const string_view &type,
                            const uint64_t &depth,
                            const event::idx &event_idx)
{
	assert(room_id);
	mutable_buffer out{out_};
	consume(out, copy(out, room_id));

	if(!type)
		return { data(out_), data(out) };

	consume(out, copy(out, '\0'));
	consume(out, copy(out, trunc(type, event::TYPE_MAX_SIZE)));
	consume(out, copy(out, '\0'));
	consume(out, copy(out, byte_view<string_view>(depth)));
	consume(out, copy(out, byte_view<string_view>(event_idx)));
	return { data(out_), data(out) };
}
