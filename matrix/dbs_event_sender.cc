// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

decltype(ircd::m::dbs::event_sender)
ircd::m::dbs::event_sender;

decltype(ircd::m::dbs::desc::event_sender__comp)
ircd::m::dbs::desc::event_sender__comp
{
	{ "name",     "ircd.m.dbs._event_sender.comp" },
	{ "default",  "default"                       },
};

decltype(ircd::m::dbs::desc::event_sender__block__size)
ircd::m::dbs::desc::event_sender__block__size
{
	{ "name",     "ircd.m.dbs._event_sender.block.size" },
	{ "default",  512L                                  },
};

decltype(ircd::m::dbs::desc::event_sender__meta_block__size)
ircd::m::dbs::desc::event_sender__meta_block__size
{
	{ "name",     "ircd.m.dbs._event_sender.meta_block.size" },
	{ "default",  4096L                                      },
};

decltype(ircd::m::dbs::desc::event_sender__cache__size)
ircd::m::dbs::desc::event_sender__cache__size
{
	{
		{ "name",     "ircd.m.dbs._event_sender.cache.size" },
		{ "default",  long(16_MiB)                          },
	}, []
	{
		const size_t &value{event_sender__cache__size};
		db::capacity(db::cache(dbs::event_sender), value);
	}
};

decltype(ircd::m::dbs::desc::event_sender__cache_comp__size)
ircd::m::dbs::desc::event_sender__cache_comp__size
{
	{
		{ "name",     "ircd.m.dbs._event_sender.cache_comp.size" },
		{ "default",  long(0_MiB)                                },
	}, []
	{
		const size_t &value{event_sender__cache_comp__size};
		db::capacity(db::cache_compressed(dbs::event_sender), value);
	}
};

const ircd::db::prefix_transform
ircd::m::dbs::desc::event_sender__pfx
{
	"_event_sender",
	[](const string_view &key)
	{
		return startswith(key, '@')?
			has(key, '\0'):
			has(key, '@');
	},

	[](const string_view &key)
	{
		const auto &[prefix, suffix]
		{
			// Split @localpart:hostpart\0event_idx by '\0'
			startswith(key, '@')?
				split(key, '\0'):

			// Split hostpart@localpart\0event_idx by '@'
				split(key, '@')
		};

		return prefix;
	}
};

const ircd::db::descriptor
ircd::m::dbs::desc::event_sender
{
	// name
	"_event_sender",

	// explanation
	R"(Index of senders to their events.

	mxid | event_idx => --
	origin | localpart, event_idx => --

	The senders of events are indexed by this column. This allows for all
	events from a sender to be iterated. Additionally, all events from a
	server and all known servers can be iterated from this column.

	key #1:
	The first type of key is made from a user mxid and an event_idx concat.

	key #2:
	The second type of key is made from a user mxid and an event_id, where
	the mxid is part-swapped so the origin comes first, and the @localpart
	comes after.

	Note that the indexers of this column ignores the actual "origin" field
	of an event. Only the "sender" data is used here.
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
	event_sender__pfx,

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
	size_t(event_sender__block__size),

	// meta_block size
	size_t(event_sender__meta_block__size),

	// compression
	string_view{event_sender__comp},

	// compactor
	{},

	// compaction priority algorithm
	"kOldestSmallestSeqFirst"s,
};

//
// indexer
//

void
ircd::m::dbs::_index_event_sender(db::txn &txn,
                                  const event &event,
                                  const write_opts &opts)
{
	assert(opts.appendix.test(appendix::EVENT_SENDER));
	assert(json::get<"sender"_>(event));
	assert(opts.event_idx);

	thread_local char buf[2][EVENT_SENDER_KEY_MAX_SIZE];
	const string_view &sender_key
	{
		event_sender_key(buf[0], at<"sender"_>(event), opts.event_idx)
	};

	const string_view &sender_origin_key
	{
		event_sender_origin_key(buf[1], at<"sender"_>(event), opts.event_idx)
	};

	db::txn::append
	{
		txn, dbs::event_sender,
		{
			opts.op, sender_key
		}
	};

	db::txn::append
	{
		txn, dbs::event_sender,
		{
			opts.op, sender_origin_key
		}
	};
}

//
// key
//

// sender_key

std::tuple<ircd::m::event::idx>
ircd::m::dbs::event_sender_key(const string_view &amalgam)
{
	const auto &parts
	{
		split(amalgam, '\0')
	};

	assert(empty(parts.first));
	return
	{
		byte_view<event::idx>(parts.second),
	};
}

ircd::string_view
ircd::m::dbs::event_sender_key(const mutable_buffer &out_,
                               const user::id &user_id,
                               const event::idx &event_idx)
{
	assert(size(out_) >= EVENT_SENDER_KEY_MAX_SIZE);
	assert(!event_idx || user_id);

	mutable_buffer out{out_};
	consume(out, copy(out, user_id));

	if(user_id && event_idx)
	{
		consume(out, copy(out, '\0'));
		consume(out, copy(out, byte_view<string_view>(event_idx)));
	}

	return { data(out_), data(out) };
}

bool
ircd::m::dbs::is_event_sender_key(const string_view &key)
{
	return empty(key) || startswith(key, '@');
}

// sender_origin_key

std::tuple<ircd::string_view, ircd::m::event::idx>
ircd::m::dbs::event_sender_origin_key(const string_view &amalgam)
{
	const auto &parts
	{
		split(amalgam, '\0')
	};

	assert(!empty(parts.first) && !empty(parts.second));
	assert(startswith(parts.first, '@'));

	return
	{
		parts.first,
		byte_view<event::idx>(parts.second),
	};
}

ircd::string_view
ircd::m::dbs::event_sender_origin_key(const mutable_buffer &out,
                                      const user::id &user_id,
                                      const event::idx &event_idx)
{
	return event_sender_origin_key(out, user_id.host(), user_id.local(), event_idx);
}

ircd::string_view
ircd::m::dbs::event_sender_origin_key(const mutable_buffer &out_,
                                      const string_view &origin,
                                      const string_view &localpart,
                                      const event::idx &event_idx)
{
	assert(size(out_) >= EVENT_SENDER_KEY_MAX_SIZE);
	assert(!event_idx || localpart);
	assert(!localpart || startswith(localpart, '@'));

	mutable_buffer out{out_};
	consume(out, copy(out, origin));
	consume(out, copy(out, localpart));

	if(localpart && event_idx)
	{
		consume(out, copy(out, '\0'));
		consume(out, copy(out, byte_view<string_view>(event_idx)));
	}

	return { data(out_), data(out) };
}

bool
ircd::m::dbs::is_event_sender_origin_key(const string_view &key)
{
	return !startswith(key, '@');
}
