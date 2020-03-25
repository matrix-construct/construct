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
	static bool room_state_space__cmp_lt(const string_view &, const string_view &);
}

decltype(ircd::m::dbs::room_state_space)
ircd::m::dbs::room_state_space;

decltype(ircd::m::dbs::desc::room_state_space__block__size)
ircd::m::dbs::desc::room_state_space__block__size
{
	{ "name",     "ircd.m.dbs._room_state_space.block.size" },
	{ "default",  512L                                      },
};

decltype(ircd::m::dbs::desc::room_state_space__meta_block__size)
ircd::m::dbs::desc::room_state_space__meta_block__size
{
	{ "name",     "ircd.m.dbs._room_state_space.meta_block.size" },
	{ "default",  long(8_KiB)                                    },
};

decltype(ircd::m::dbs::desc::room_state_space__cache__size)
ircd::m::dbs::desc::room_state_space__cache__size
{
	{
		{ "name",     "ircd.m.dbs._room_state_space.cache.size"  },
		{ "default",  long(16_MiB)                               },
	}, []
	{
		const size_t &value{room_state_space__cache__size};
		db::capacity(db::cache(dbs::room_state_space), value);
	}
};

decltype(ircd::m::dbs::desc::room_state_space__cache_comp__size)
ircd::m::dbs::desc::room_state_space__cache_comp__size
{
	{
		{ "name",     "ircd.m.dbs._room_state_space.cache_comp.size"  },
		{ "default",  long(8_MiB)                                     },
	}, []
	{
		const size_t &value{room_state_space__cache_comp__size};
		db::capacity(db::cache_compressed(dbs::room_state_space), value);
	}
};

decltype(ircd::m::dbs::desc::room_state_space__bloom__bits)
ircd::m::dbs::desc::room_state_space__bloom__bits
{
	{ "name",     "ircd.m.dbs._room_state_space.bloom.bits" },
	{ "default",  10L                                       },
};

const ircd::db::comparator
ircd::m::dbs::desc::room_state_space__cmp
{
	"_room_state_space",
	room_state_space__cmp_lt,
	std::equal_to<string_view>{},
};

const ircd::db::prefix_transform
ircd::m::dbs::desc::room_state_space__pfx
{
	"_room_state_space",

	[](const string_view &key)
	{
		return has(key, "\0"_sv);
	},

	[](const string_view &key)
	{
		return split(key, "\0"_sv).first;
	}
};

const ircd::db::descriptor
ircd::m::dbs::desc::room_state_space
{
	// name
	"_room_state_space",

	// explanation
	R"(All states of the room.

	)",

	// typing (key, value)
	{
		typeid(string_view), typeid(uint64_t)
	},

	// options
	{},

	// comparator
	room_state_space__cmp,

	// prefix transform
	room_state_space__pfx,

	// drop column
	false,

	// cache size
	bool(cache_enable)? -1 : 0,

	// cache size for compressed assets
	bool(cache_comp_enable)? -1 : 0,

	// bloom filter bits
	size_t(room_state_space__bloom__bits),

	// expect queries hit
	false,

	// block size
	size_t(room_state_space__block__size),

	// meta_block size
	size_t(room_state_space__meta_block__size),

	// compression
	"kLZ4Compression;kSnappyCompression"s,

	// compactor
	{},

	// compaction priority algorithm
	"kOldestSmallestSeqFirst"s,
};

//
// indexer
//

void
ircd::m::dbs::_index_room_state_space(db::txn &txn,
                                      const event &event,
                                      const write_opts &opts)
{
	assert(opts.appendix.test(appendix::ROOM_STATE_SPACE));

	const ctx::critical_assertion ca;
	thread_local char buf[ROOM_STATE_SPACE_KEY_MAX_SIZE];
	const string_view &key
	{
		room_state_space_key(buf, at<"room_id"_>(event), at<"type"_>(event), at<"state_key"_>(event), at<"depth"_>(event), opts.event_idx)
	};

	db::txn::append
	{
		txn, room_state_space,
		{
			opts.op,
			key,
		}
	};
}

//
// cmp
//

bool
ircd::m::dbs::room_state_space__cmp_lt(const string_view &a,
                                       const string_view &b)
{
	static const auto &pt
	{
		desc::room_state_space__pfx
	};

	const string_view pre[2]
	{
		pt.get(a),
		pt.get(b),
	};

	if(size(pre[0]) != size(pre[1]))
		return size(pre[0]) < size(pre[1]);

	if(pre[0] != pre[1])
		return pre[0] < pre[1];

	const string_view post[2]
	{
		a.substr(size(pre[0])),
		b.substr(size(pre[1])),
	};

	// These conditions are matched on some queries when the user only
	// supplies a room_id.
	if(empty(post[0]))
		return true;

	if(empty(post[1]))
		return false;

	// Decompose the postfix of the key for granular sorting
	const room_state_space_key_parts k[2]
	{
		room_state_space_key(post[0]),
		room_state_space_key(post[1])
	};

	// type
	if(std::get<0>(k[0]) < std::get<0>(k[1]))
		return true;
	else if(std::get<0>(k[0]) > std::get<0>(k[1]))
		return false;

	// state_key
	if(std::get<1>(k[0]) < std::get<1>(k[1]))
		return true;
	else if(std::get<1>(k[0]) > std::get<1>(k[1]))
		return false;

	// depth (ORDER IS DESCENDING!)
	if(uint64_t(std::get<2>(k[0])) > uint64_t(std::get<2>(k[1])))
		return true;
	else if(uint64_t(std::get<2>(k[0])) < uint64_t(std::get<2>(k[1])))
		return false;

	// event_idx (ORDER IS DESCENDING!)
	if(std::get<3>(k[0]) > std::get<3>(k[1]))
		return true;
	else if(std::get<3>(k[0]) < std::get<3>(k[1]))
		return false;

	return false;
}

//
// key
//

ircd::m::dbs::room_state_space_key_parts
ircd::m::dbs::room_state_space_key(const string_view &amalgam)
{
	const auto &key
	{
		lstrip(amalgam, "\0"_sv)
	};

	const auto &[type, after_type]
	{
		split(key, "\0"_sv)
	};

	const auto &[state_key, after_state_key]
	{
		split(after_type, "\0"_sv)
	};

	const int64_t &depth
	{
		size(after_state_key) >= 8?
			int64_t(byte_view<int64_t>(after_state_key.substr(0, 8))):
			-1L
	};

	const event::idx &event_idx
	{
		size(after_state_key) >= 16?
			event::idx(byte_view<event::idx>(after_state_key.substr(8, 8))):
			0UL
	};

	return
	{
		type, state_key, depth, event_idx
	};
}

ircd::string_view
ircd::m::dbs::room_state_space_key(const mutable_buffer &out_,
                                   const id::room &room_id)
{
	return room_state_space_key(out_, room_id, string_view{}, string_view{}, -1L, 0L);
}

ircd::string_view
ircd::m::dbs::room_state_space_key(const mutable_buffer &out_,
                                   const id::room &room_id,
                                   const string_view &type)
{
	return room_state_space_key(out_, room_id, type, string_view{}, -1L, 0L);
}

ircd::string_view
ircd::m::dbs::room_state_space_key(const mutable_buffer &out_,
                                   const id::room &room_id,
                                   const string_view &type,
                                   const string_view &state_key)
{
	return room_state_space_key(out_, room_id, type, state_key, -1L, 0L);
}

ircd::string_view
ircd::m::dbs::room_state_space_key(const mutable_buffer &out_,
                                   const id::room &room_id,
                                   const string_view &type,
                                   const string_view &state_key,
                                   const int64_t &depth,
                                   const event::idx &event_idx)
{
	mutable_buffer out{out_};
	consume(out, copy(out, room_id));
	consume(out, copy(out, "\0"_sv));
	consume(out, copy(out, type));
	consume(out, copy(out, "\0"_sv));
	consume(out, copy(out, state_key));
	consume(out, copy(out, "\0"_sv));
	consume(out, copy(out, byte_view<string_view>(depth)));
	consume(out, copy(out, byte_view<string_view>(event_idx)));
	return { data(out_), data(out) };
}
