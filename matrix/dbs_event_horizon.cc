// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

decltype(ircd::m::dbs::event_horizon)
ircd::m::dbs::event_horizon;

decltype(ircd::m::dbs::desc::event_horizon__comp)
ircd::m::dbs::desc::event_horizon__comp
{
	{ "name",     "ircd.m.dbs._event_horizon.comp" },
	{ "default",  "default"                        },
};

decltype(ircd::m::dbs::desc::event_horizon__block__size)
ircd::m::dbs::desc::event_horizon__block__size
{
	{ "name",     "ircd.m.dbs._event_horizon.block.size" },
	{ "default",  512L                                   },
};

decltype(ircd::m::dbs::desc::event_horizon__meta_block__size)
ircd::m::dbs::desc::event_horizon__meta_block__size
{
	{ "name",     "ircd.m.dbs._event_horizon.meta_block.size" },
	{ "default",  1024L                                       },
};

decltype(ircd::m::dbs::desc::event_horizon__cache__size)
ircd::m::dbs::desc::event_horizon__cache__size
{
	{
		{ "name",     "ircd.m.dbs._event_horizon.cache.size" },
		{ "default",  long(16_MiB)                           },
	}, []
	{
		const size_t &value{event_horizon__cache__size};
		db::capacity(db::cache(dbs::event_horizon), value);
	}
};

decltype(ircd::m::dbs::desc::event_horizon__cache_comp__size)
ircd::m::dbs::desc::event_horizon__cache_comp__size
{
	{
		{ "name",     "ircd.m.dbs._event_horizon.cache_comp.size" },
		{ "default",  long(0_MiB)                                 },
	}, []
	{
		const size_t &value{event_horizon__cache_comp__size};
		db::capacity(db::cache_compressed(dbs::event_horizon), value);
	}
};

const ircd::db::prefix_transform
ircd::m::dbs::desc::event_horizon__pfx
{
	"_event_horizon",

	[](const string_view &key)
	{
		return has(key, '\0');
	},

	[](const string_view &key)
	{
		assert(size(key) >= sizeof(event::idx));
		return split(key, '\0').first;
	}
};

const ircd::db::descriptor
ircd::m::dbs::desc::event_horizon
{
	// name
	"_event_horizon",

	// explanation
	R"(Unresolved references in the reverse reference graph of events.

	event_id | event_idx => --

	The first part of the key is an event_id which the server does not have.
	The suffix of the key is the index number of an event which the server
	does have and it contains a reference to event_id.

	We use the information in this column to find all of the events which
	have an unresolved reference to this event and complete the holes in the
	event_refs graph which could not be completed without this event.

	When a new event is written to the database the event_horizon column is
	queried seeking the event's ID. Each entry in event_horizon is the index
	of an event which we previously wrote to the database without knowing the
	index of the event currently being written (an out-of-order write).

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
	event_horizon__pfx,

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
	size_t(event_horizon__block__size),

	// meta_block size
	size_t(event_horizon__meta_block__size),

	// compression
	string_view{event_horizon__comp},

	// compactor
	{},

	// compaction priority algorithm
	"kOldestSmallestSeqFirst"s,
};

//
// indexer
//

namespace ircd::m::dbs
{
	static void _index_event_horizon_resolve_one(db::txn &, const event &, const write_opts &, const event::idx &);
}

// NOTE: QUERY
void
ircd::m::dbs::_index_event_horizon_resolve(db::txn &txn,
                                           const event &event,
                                           const write_opts &opts)
{
	assert(opts.appendix.test(appendix::EVENT_HORIZON_RESOLVE));
	assert(opts.event_idx != 0);
	assert(event.event_id);

	char buf[EVENT_HORIZON_KEY_MAX_SIZE];
	const string_view &key
	{
		event_horizon_key(buf, event.event_id)
	};

	auto it(dbs::event_horizon.begin(key)); do
	{
		static const size_t max{32};

		size_t num(0);
		event::idx event_idx[max];
		for(; num < max && it; ++it)
		{
			event_idx[num] = std::get<0>(event_horizon_key(it->first));
			num += event_idx[num] != 0;
		}

		for(size_t i(0); i < num; ++i)
			_index_event_horizon_resolve_one(txn, event, opts, event_idx[i]);
	}
	while(it);
}

size_t
ircd::m::dbs::_prefetch_event_horizon_resolve(const event &event,
                                              const write_opts &opts)
{
	assert(opts.appendix.test(appendix::EVENT_HORIZON_RESOLVE));

	size_t ret(0);
	if(!event.event_id)
		return ret;

	char buf[EVENT_HORIZON_KEY_MAX_SIZE];
	assert(event.event_id);
	const string_view &key
	{
		event_horizon_key(buf, event.event_id)
	};

	for(auto it(dbs::event_horizon.begin(key)); it; ++it)
	{
		const auto event_idx
		{
			std::get<0>(event_horizon_key(it->first))
		};

		ret += m::prefetch(event_idx);
	}

	return ret;
}

// NOTE: QUERY
void
ircd::m::dbs::_index_event_horizon_resolve_one(db::txn &txn,
                                               const event &event,
                                               const write_opts &opts,
                                               const event::idx &event_idx)
{
	assert(event_idx != 0);
	assert(event_idx != opts.event_idx);

	const event::fetch _event
	{
		std::nothrow, event_idx
	};

	if(!_event.valid)
	{
		log::dwarning
		{
			log, "Horizon resolve for %s @%lu not possible @%lu",
			string_view{event.event_id},
			opts.event_idx,
			event_idx,
		};

		return;
	}

	log::debug
	{
		log, "Horizon resolve for %s @%lu; remisé %s @%lu",
		string_view{event.event_id},
		opts.event_idx,
		string_view{_event.event_id},
		event_idx,
	};

	// Make the references on behalf of the future event
	write_opts _opts;
	_opts.op = opts.op;
	_opts.event_idx = event_idx;
	_opts.appendix.reset();
	_opts.appendix.set(appendix::EVENT_REFS, opts.appendix[appendix::EVENT_REFS]);
	_opts.appendix.set(appendix::ROOM_REDACT, opts.appendix[appendix::ROOM_REDACT]);
	_opts.appendix.set(appendix::ROOM_HEAD_RESOLVE, opts.appendix[appendix::ROOM_HEAD_RESOLVE]);
	_opts.event_refs = opts.horizon_resolve;
	_opts.interpose = &txn;
	write(txn, _event, _opts);

	// Delete the event_horizon entry after resolving.
	thread_local char buf[EVENT_HORIZON_KEY_MAX_SIZE];
	const string_view &key
	{
		event_horizon_key(buf, event.event_id, event_idx)
	};

	db::txn::append
	{
		txn, dbs::event_horizon,
		{
			opts.op == db::op::SET?
				db::op::DELETE:
				db::op::SET,
			key
		}
	};
}

void
ircd::m::dbs::_index_event_horizon(db::txn &txn,
                                   const event &event,
                                   const write_opts &opts,
                                   const m::event::id &unresolved_id)
{
	thread_local char buf[EVENT_HORIZON_KEY_MAX_SIZE];
	assert(opts.appendix.test(appendix::EVENT_HORIZON));
	assert(opts.event_idx != 0 && unresolved_id);
	const string_view &key
	{
		event_horizon_key(buf, unresolved_id, opts.event_idx)
	};

	db::txn::append
	{
		txn, dbs::event_horizon,
		{
			opts.op, key
		}
	};
}

//
// key
//

std::tuple<ircd::m::event::idx>
ircd::m::dbs::event_horizon_key(const string_view &amalgam)
{
	assert(size(amalgam) == 1 + sizeof(event::idx));
	assert(amalgam[0] == '\0');

	const byte_view<event::idx> &event_idx
	{
		amalgam.substr(1)
	};

	return
	{
		static_cast<event::idx>(event_idx)
	};
}

ircd::string_view
ircd::m::dbs::event_horizon_key(const mutable_buffer &out,
                                const event::id &event_id)
{
	return event_horizon_key(out, event_id, 0UL);
}

ircd::string_view
ircd::m::dbs::event_horizon_key(const mutable_buffer &out,
                                const event::id &event_id,
                                const event::idx &event_idx)
{
	mutable_buffer buf(out);
	consume(buf, copy(buf, event_id));

	if(event_idx)
	{
		consume(buf, copy(buf, '\0'));
		consume(buf, copy(buf, byte_view<string_view>(event_idx)));
	}

	const string_view ret
	{
		data(out), data(buf)
	};

	assert(size(ret) == size(event_id) || size(ret) == size(event_id) + sizeof(event::idx) + 1);
	return ret;
}
