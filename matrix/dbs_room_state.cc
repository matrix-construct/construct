// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

decltype(ircd::m::dbs::room_state)
ircd::m::dbs::room_state;

decltype(ircd::m::dbs::desc::room_state__comp)
ircd::m::dbs::desc::room_state__comp
{
	{ "name",     "ircd.m.dbs._room_state.comp" },
	{ "default",  "default"                     },
};

decltype(ircd::m::dbs::desc::room_state__block__size)
ircd::m::dbs::desc::room_state__block__size
{
	{ "name",     "ircd.m.dbs._room_state.block.size" },
	{ "default",  512L                                },
};

decltype(ircd::m::dbs::desc::room_state__meta_block__size)
ircd::m::dbs::desc::room_state__meta_block__size
{
	{ "name",     "ircd.m.dbs._room_state.meta_block.size" },
	{ "default",  8192L                                    },
};

decltype(ircd::m::dbs::desc::room_state__cache__size)
ircd::m::dbs::desc::room_state__cache__size
{
	{
		{ "name",     "ircd.m.dbs._room_state.cache.size"  },
		{ "default",  long(32_MiB)                         },
	}, []
	{
		const size_t &value{room_state__cache__size};
		db::capacity(db::cache(dbs::room_state), value);
	}
};

decltype(ircd::m::dbs::desc::room_state__cache_comp__size)
ircd::m::dbs::desc::room_state__cache_comp__size
{
	{
		{ "name",     "ircd.m.dbs._room_state.cache_comp.size"  },
		{ "default",  long(8_MiB)                               },
	}, []
	{
		const size_t &value{room_state__cache_comp__size};
		db::capacity(db::cache_compressed(dbs::room_state), value);
	}
};

decltype(ircd::m::dbs::desc::room_state__bloom__bits)
ircd::m::dbs::desc::room_state__bloom__bits
{
	{ "name",     "ircd.m.dbs._room_state.bloom.bits" },
	{ "default",  0L                                  },
};

/// prefix transform for type,state_key in room_id
///
/// This transform is special for concatenating room_id with type and state_key
/// in that order with prefix being the room_id (this may change to room_id+
/// type
///
const ircd::db::prefix_transform
ircd::m::dbs::desc::room_state__pfx
{
	"_room_state",

	[](const string_view &key)
	{
		return has(key, "\0"_sv);
	},

	[](const string_view &key)
	{
		return split(key, '\0').first;
	}
};

const ircd::db::descriptor
ircd::m::dbs::desc::room_state
{
	// name
	"_room_state",

	// explanation
	R"(The present state of the room.

	[room_id | type + state_key] => event_idx

	This column is also known as the "present state table." It contains the
	very important present state of the room for this server. The key contains
	plaintext room_id, type and state_key elements for direct point-lookup as
	well as iteration. The value is the index of the apropos state event.

	)",

	// typing (key, value)
	{
		typeid(string_view), typeid(uint64_t)
	},

	// options
	{},

	// comparator
	{},

	// prefix transform
	room_state__pfx,

	// drop column
	false,

	// cache size
	bool(cache_enable)? -1 : 0,

	// cache size for compressed assets
	bool(cache_comp_enable)? -1 : 0,

	// bloom filter bits
	size_t(room_state__bloom__bits),

	// expect queries hit
	false,

	// block size
	size_t(room_state__block__size),

	// meta_block size
	size_t(room_state__meta_block__size),

	// compression
	string_view{room_state__comp},

	// compactor
	{},

	// compaction priority algorithm
	"kOldestSmallestSeqFirst"s,
};

//
// indexer
//

void
ircd::m::dbs::_index_room_state(db::txn &txn,
                                const event &event,
                                const write_opts &opts)
{
	assert(opts.appendix.test(appendix::ROOM_STATE));

	const ctx::critical_assertion ca;
	thread_local char buf[ROOM_STATE_KEY_MAX_SIZE];
	const string_view &key
	{
		room_state_key(buf, at<"room_id"_>(event), at<"type"_>(event), at<"state_key"_>(event))
	};

	const string_view val
	{
		byte_view<string_view>(opts.event_idx)
	};

	db::txn::append
	{
		txn, room_state,
		{
			opts.op,
			key,
			value_required(opts.op)? val : string_view{},
		}
	};
}

//
// key
//

ircd::string_view
ircd::m::dbs::room_state_key(const mutable_buffer &out_,
                             const id::room &room_id,
                             const string_view &type)
{
	return room_state_key(out_, room_id, type, string_view{});
}

ircd::string_view
ircd::m::dbs::room_state_key(const mutable_buffer &out_,
                             const id::room &room_id,
                             const string_view &type,
                             const string_view &state_key)
{
	mutable_buffer out{out_};
	consume(out, copy(out, room_id));
	assert(room_id);

	if(likely(defined(type)))
	{
		consume(out, copy(out, '\0'));
		consume(out, copy(out, trunc(type, event::TYPE_MAX_SIZE)));
	}

	if(defined(state_key))
	{
		consume(out, copy(out, '\0'));
		consume(out, copy(out, trunc(state_key, event::STATE_KEY_MAX_SIZE)));
	}

	return { data(out_), data(out) };
}

std::tuple<ircd::string_view, ircd::string_view>
ircd::m::dbs::room_state_key(const string_view &amalgam)
{
	const auto &key
	{
		lstrip(amalgam, '\0')
	};

	const auto &s
	{
		split(key, '\0')
	};

	return
	{
		s.first, s.second
	};
}
