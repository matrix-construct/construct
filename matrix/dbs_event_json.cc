// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

decltype(ircd::m::dbs::event_json)
ircd::m::dbs::event_json;

decltype(ircd::m::dbs::desc::event_json__comp)
ircd::m::dbs::desc::event_json__comp
{
	{ "name",     "ircd.m.dbs._event_json.comp" },
	{ "default",  "default"                     },
};

decltype(ircd::m::dbs::desc::event_json__block__size)
ircd::m::dbs::desc::event_json__block__size
{
	{ "name",     "ircd.m.dbs._event_json.block.size" },
	{ "default",  long(1_KiB)                         },
};

decltype(ircd::m::dbs::desc::event_json__meta_block__size)
ircd::m::dbs::desc::event_json__meta_block__size
{
	{ "name",     "ircd.m.dbs._event_json.meta_block.size" },
	{ "default",  512L                                     },
};

decltype(ircd::m::dbs::desc::event_json__cache__size)
ircd::m::dbs::desc::event_json__cache__size
{
	{
		{ "name",     "ircd.m.dbs._event_json.cache.size" },
		{ "default",  long(128_MiB)                       },
	}, []
	{
		const size_t &value{event_json__cache__size};
		db::capacity(db::cache(dbs::event_json), value);
	}
};

decltype(ircd::m::dbs::desc::event_json__cache_comp__size)
ircd::m::dbs::desc::event_json__cache_comp__size
{
	{
		{ "name",     "ircd.m.dbs._event_json.cache_comp.size" },
		{ "default",  long(0_MiB)                              },
	}, []
	{
		const size_t &value{event_json__cache_comp__size};
		db::capacity(db::cache_compressed(dbs::event_json), value);
	}
};

decltype(ircd::m::dbs::desc::event_json__bloom__bits)
ircd::m::dbs::desc::event_json__bloom__bits
{
	{ "name",     "ircd.m.dbs._event_json.bloom.bits" },
	{ "default",  0L                                  },
};

decltype(ircd::m::dbs::desc::event_json__file__size__max)
ircd::m::dbs::desc::event_json__file__size__max
{
	{ "name",     "ircd.m.dbs._event_json.file.size.target" },
	{ "default",  long(512_MiB)                             },
};

const ircd::db::descriptor
ircd::m::dbs::desc::event_json
{
	// name
	"_event_json",

	// explanation
	R"(Full JSON object of an event.

	event_idx => event_json

	)",

	// typing (key, value)
	{
		typeid(uint64_t), typeid(string_view)
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
	size_t(event_json__bloom__bits),

	// expect queries hit
	true,

	// block size
	size_t(event_json__block__size),

	// meta_block size
	size_t(event_json__meta_block__size),

	// compression
	string_view{event_json__comp},

	// compactor
	{},

	// compaction priority algorithm
	"Universal"s,

	// target_file_size
	{
		size_t(event_json__file__size__max),  // base
		1L,                                   // multiplier
	},
};

//
// indexer
//

void
ircd::m::dbs::_index_event_json(db::txn &txn,
                                const event &event,
                                const write_opts &opts)
{
	const ctx::critical_assertion ca;
	assert(opts.appendix.test(appendix::EVENT_JSON));
	assert(opts.event_idx);

	const string_view &key
	{
		byte_view<string_view>(opts.event_idx)
	};

	const string_view &val
	{
		// If an already-strung json::object is carried by the event and
		// the opts allow us, we use it directly. This is not the default
		// path unless the developer knows the source JSON is good enough
		// to store directly.
		opts.op == db::op::SET && event.source && opts.json_source?
			string_view{event.source}:

		// If an already-strung json::object is carried by the event we
		// re-stringify it into a temporary buffer. This is the common case
		// because the original source might be crap JSON w/ spaces etc.
		opts.op == db::op::SET && event.source?
			json::stringify(mutable_buffer{event::buf[0]}, event.source):

		// If no source was given with the event we can generate it.
		opts.op == db::op::SET?
			json::stringify(mutable_buffer{event::buf[0]}, event):

		// Empty value; generally for a non-SET db::op
		string_view{}
	};

	db::txn::append
	{
		txn, event_json,
		{
			opts.op,   // db::op
			key,       // key
			val,       // val
		}
	};
}
