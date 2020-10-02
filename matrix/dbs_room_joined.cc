// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

decltype(ircd::m::dbs::room_joined)
ircd::m::dbs::room_joined;

decltype(ircd::m::dbs::desc::room_joined__comp)
ircd::m::dbs::desc::room_joined__comp
{
	{ "name",     "ircd.m.dbs._room_joined.comp" },
	{ "default",  "default"                      },
};

decltype(ircd::m::dbs::desc::room_joined__block__size)
ircd::m::dbs::desc::room_joined__block__size
{
	{ "name",     "ircd.m.dbs._room_joined.block.size" },
	{ "default",  512L                                 },
};

decltype(ircd::m::dbs::desc::room_joined__meta_block__size)
ircd::m::dbs::desc::room_joined__meta_block__size
{
	{ "name",     "ircd.m.dbs._room_joined.meta_block.size" },
	{ "default",  long(8_KiB)                               },
};

decltype(ircd::m::dbs::desc::room_joined__cache__size)
ircd::m::dbs::desc::room_joined__cache__size
{
	{
		{ "name",     "ircd.m.dbs._room_joined.cache.size" },
		{ "default",  long(8_MiB)                          },
	}, []
	{
		const size_t &value{room_joined__cache__size};
		db::capacity(db::cache(dbs::room_joined), value);
	}
};

decltype(ircd::m::dbs::desc::room_joined__cache_comp__size)
ircd::m::dbs::desc::room_joined__cache_comp__size
{
	{
		{ "name",     "ircd.m.dbs._room_joined.cache_comp.size" },
		{ "default",  long(8_MiB)                               },
	}, []
	{
		const size_t &value{room_joined__cache_comp__size};
		db::capacity(db::cache_compressed(dbs::room_joined), value);
	}
};

decltype(ircd::m::dbs::desc::room_joined__bloom__bits)
ircd::m::dbs::desc::room_joined__bloom__bits
{
	{ "name",     "ircd.m.dbs._room_joined.bloom.bits" },
	{ "default",  0L                                   },
};

/// Prefix transform for the room_joined
///
const ircd::db::prefix_transform
ircd::m::dbs::desc::room_joined__pfx
{
	"_room_joined",

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
ircd::m::dbs::desc::room_joined
{
	// name
	"_room_joined",

	// explanation
	R"(Specifically indexes joined members of a room for fast iteration.

	[room_id | origin + mxid] => event_idx

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
	room_joined__pfx,

	// drop column
	false,

	// cache size
	bool(cache_enable)? -1 : 0,

	// cache size for compressed assets
	bool(cache_comp_enable)? -1 : 0,

	// bloom filter bits
	size_t(room_joined__bloom__bits),

	// expect queries hit
	false,

	// block size
	size_t(room_joined__block__size),

	// meta_block size
	size_t(room_joined__meta_block__size),

	// compression
	string_view{room_joined__comp},

	// compactor
	{},

	// compaction priority algorithm
	"kOldestSmallestSeqFirst"s,
};

//
// indexer
//

/// Adds the entry for the room_joined column into the txn.
void
ircd::m::dbs::_index_room_joined(db::txn &txn,
                                  const event &event,
                                  const write_opts &opts)
{
	assert(opts.appendix.test(appendix::ROOM_JOINED));
	assert(at<"type"_>(event) == "m.room.member");

	thread_local char buf[ROOM_JOINED_KEY_MAX_SIZE];
	const ctx::critical_assertion ca;
	const string_view &key
	{
		room_joined_key(buf, at<"room_id"_>(event), at<"origin"_>(event), at<"state_key"_>(event))
	};

	const string_view &membership
	{
		m::membership(event)
	};

	assert(!empty(membership));

	db::op op;
	if(opts.op == db::op::SET) switch(hash(membership))
	{
		case hash("join"):
			op = db::op::SET;
			break;

		case hash("ban"):
		case hash("leave"):
			op = db::op::DELETE;
			break;

		default:
			return;
	}
	else if(opts.op == db::op::DELETE)
		op = opts.op;
	else
		return;

	const string_view val
	{
		byte_view<string_view>{opts.event_idx}
	};

	assert(val.size() >= sizeof(event::idx));
	db::txn::append
	{
		txn, room_joined,
		{
			op,
			key,
			val,
		}
	};
}

//
// key
//

std::tuple<ircd::string_view, ircd::string_view>
ircd::m::dbs::room_joined_key(const string_view &amalgam)
{
	const auto &key
	{
		lstrip(amalgam, '\0')
	};

	const auto &s
	{
		split(key, '@')
	};

	return
	{
		{ s.first },
		!empty(s.second)?
			string_view{begin(s.second) - 1, end(s.second)}:
			string_view{}
	};
}

ircd::string_view
ircd::m::dbs::room_joined_key(const mutable_buffer &out_,
                              const id::room &room_id,
                              const string_view &origin)
{
	mutable_buffer out{out_};
	consume(out, copy(out, room_id));
	consume(out, copy(out, '\0'));
	consume(out, copy(out, trunc(origin, event::ORIGIN_MAX_SIZE)));
	return { data(out_), data(out) };
}

ircd::string_view
ircd::m::dbs::room_joined_key(const mutable_buffer &out_,
                              const id::room &room_id,
                              const string_view &origin,
                              const id::user &member)
{
	mutable_buffer out{out_};
	consume(out, copy(out, room_id));
	consume(out, copy(out, '\0'));
	consume(out, copy(out, trunc(origin, event::ORIGIN_MAX_SIZE)));
	consume(out, copy(out, member));
	return { data(out_), data(out) };
}
