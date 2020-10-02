// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

decltype(ircd::m::dbs::event_type)
ircd::m::dbs::event_type;

decltype(ircd::m::dbs::desc::event_type__comp)
ircd::m::dbs::desc::event_type__comp
{
	{ "name",     "ircd.m.dbs._event_type.comp" },
	{ "default",  "default"                     },
};

decltype(ircd::m::dbs::desc::event_type__block__size)
ircd::m::dbs::desc::event_type__block__size
{
	{ "name",     "ircd.m.dbs._event_type.block.size" },
	{ "default",  512L                                },
};

decltype(ircd::m::dbs::desc::event_type__meta_block__size)
ircd::m::dbs::desc::event_type__meta_block__size
{
	{ "name",     "ircd.m.dbs._event_type.meta_block.size" },
	{ "default",  long(4_KiB)                              },
};

decltype(ircd::m::dbs::desc::event_type__cache__size)
ircd::m::dbs::desc::event_type__cache__size
{
	{
		{ "name",     "ircd.m.dbs._event_type.cache.size" },
		{ "default",  long(16_MiB)                        },
	}, []
	{
		const size_t &value{event_type__cache__size};
		db::capacity(db::cache(dbs::event_type), value);
	}
};

decltype(ircd::m::dbs::desc::event_type__cache_comp__size)
ircd::m::dbs::desc::event_type__cache_comp__size
{
	{
		{ "name",     "ircd.m.dbs._event_type.cache_comp.size" },
		{ "default",  long(0_MiB)                              },
	}, []
	{
		const size_t &value{event_type__cache_comp__size};
		db::capacity(db::cache_compressed(dbs::event_type), value);
	}
};

const ircd::db::prefix_transform
ircd::m::dbs::desc::event_type__pfx
{
	"_event_type",

	[](const string_view &key)
	{
		return has(key, '\0');
	},

	[](const string_view &key)
	{
		return split(key, '\0').first;
	}
};

const ircd::db::descriptor
ircd::m::dbs::desc::event_type
{
	// name
	"_event_type",

	// explanation
	R"(Index of types of events.

	type | event_idx => --

	The types of events are indexed by this column. All events of a specific type can be
	iterated efficiently. The type string forms the prefix domain.

	)",

	// typing (key, value)
	{
		typeid(string_view), typeid(string_view)
	},

	// options
	{},

	// comparator
	{},

	// prefix transform
	event_type__pfx,

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
	size_t(event_type__block__size),

	// meta_block size
	size_t(event_type__meta_block__size),

	// compression
	string_view{event_type__comp},

	// compactor
	{},

	// compaction priority algorithm
	"kOldestSmallestSeqFirst"s,
};

//
// indexer
//

void
ircd::m::dbs::_index_event_type(db::txn &txn,
                                const event &event,
                                const write_opts &opts)
{
	assert(opts.appendix.test(appendix::EVENT_TYPE));
	assert(json::get<"type"_>(event));
	assert(opts.event_idx);

	thread_local char buf[EVENT_TYPE_KEY_MAX_SIZE];
	const string_view &key
	{
		event_type_key(buf, at<"type"_>(event), opts.event_idx)
	};

	db::txn::append
	{
		txn, dbs::event_type,
		{
			opts.op, key
		}
	};
}

//
// key
//

std::tuple<ircd::m::event::idx>
ircd::m::dbs::event_type_key(const string_view &amalgam)
{
	assert(size(amalgam) == sizeof(event::idx) + 1);
	const auto &key
	{
		amalgam.substr(1)
	};

	assert(size(key) == sizeof(event::idx));
	return
	{
		byte_view<event::idx>(key)
	};
}

ircd::string_view
ircd::m::dbs::event_type_key(const mutable_buffer &out_,
                             const string_view &type,
                             const event::idx &event_idx)
{
	assert(size(out_) >= EVENT_TYPE_KEY_MAX_SIZE);

	mutable_buffer out{out_};
	consume(out, copy(out, trunc(type, event::TYPE_MAX_SIZE)));
	consume(out, copy(out, '\0'));
	consume(out, copy(out, byte_view<string_view>(event_idx)));
	return { data(out_), data(out) };
}
