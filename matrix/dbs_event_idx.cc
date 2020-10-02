// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

decltype(ircd::m::dbs::event_idx)
ircd::m::dbs::event_idx;

decltype(ircd::m::dbs::desc::event_idx__comp)
ircd::m::dbs::desc::event_idx__comp
{
	{ "name",     "ircd.m.dbs._event_idx.comp" },
	{ "default",  "default"                    },
};

decltype(ircd::m::dbs::desc::event_idx__block__size)
ircd::m::dbs::desc::event_idx__block__size
{
	{ "name",     "ircd.m.dbs._event_idx.block.size" },
	{ "default",  256L                               },
};

decltype(ircd::m::dbs::desc::event_idx__meta_block__size)
ircd::m::dbs::desc::event_idx__meta_block__size
{
	{ "name",     "ircd.m.dbs._event_idx.meta_block.size" },
	{ "default",  2048L                                   },
};

decltype(ircd::m::dbs::desc::event_idx__cache__size)
ircd::m::dbs::desc::event_idx__cache__size
{
	{
		{ "name",     "ircd.m.dbs._event_idx.cache.size" },
		{ "default",  long(128_MiB)                      },
	}, []
	{
		const size_t &value{event_idx__cache__size};
		db::capacity(db::cache(dbs::event_idx), value);
	}
};

decltype(ircd::m::dbs::desc::event_idx__cache_comp__size)
ircd::m::dbs::desc::event_idx__cache_comp__size
{
	{
		{ "name",     "ircd.m.dbs._event_idx.cache_comp.size" },
		{ "default",  long(16_MiB)                            },
	}, []
	{
		const size_t &value{event_idx__cache_comp__size};
		db::capacity(db::cache_compressed(dbs::event_idx), value);
	}
};

decltype(ircd::m::dbs::desc::event_idx__bloom__bits)
ircd::m::dbs::desc::event_idx__bloom__bits
{
	{ "name",     "ircd.m.dbs._event_idx.bloom.bits" },
	{ "default",  0L                                 },
};

decltype(ircd::m::dbs::desc::event_idx)
ircd::m::dbs::desc::event_idx
{
	// name
	"_event_idx",

	// explanation
	R"(Maps matrix event_id strings into internal index numbers.

	event_id => event_idx

	The key is an event_id and the value is the index number to be used as the
	key to all the event data columns. The index number is referred to as the
	event_idx and is a fixed 8 byte unsigned integer. All other columns which
	may key on an event_id string instead use this event_idx index number. The
	index number was generated sequentially based on the order the event was
	written to the database. Index numbers start at 1 because 0 is used as a
	sentinel value and is not valid. The index numbers throughout the database
	generally do not have gaps and can be iterated, however gaps may exist when
	an event is erased from the database (which is rare for the matrix
	application).

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
	{},

	// drop column
	false,

	// cache size
	bool(cache_enable)? -1 : 0, //uses conf item

	// cache size for compressed assets
	bool(cache_comp_enable)? -1 : 0,

	// bloom filter bits
	size_t(event_idx__bloom__bits),

	// expect queries hit
	false,

	// block size
	size_t(event_idx__block__size),

	// meta_block size
	size_t(event_idx__meta_block__size),

	// compression
	string_view{event_idx__comp},

	// compactor
	{},

	// compaction priority algorithm
	"kOldestSmallestSeqFirst"s,
};

//
// indexer
//

void
ircd::m::dbs::_index_event_id(db::txn &txn,
                              const event &event,
                              const write_opts &opts)
{
	assert(opts.appendix.test(appendix::EVENT_ID));
	assert(opts.event_idx);
	assert(event.event_id);

	db::txn::append
	{
		txn, dbs::event_idx,
		{
			opts.op,
			string_view{event.event_id},
			byte_view<string_view>(opts.event_idx)
		}
	};

	// For a v1 event, the "event_id" property will be saved into the `event_id`
	// column by the direct property->column indexer.
	if(json::get<"event_id"_>(event))
		return;

	// For v3+ events, the direct column indexer won't see any "event_id"
	// property. In this case we insert the `event.event_id` manually into
	// that column here.
	db::txn::append
	{
		txn, event_column.at(json::indexof<m::event, "event_id"_>()),
		{
			opts.op,
			byte_view<string_view>(opts.event_idx),
			string_view{event.event_id},
		}
	};
}
