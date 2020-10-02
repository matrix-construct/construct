// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

decltype(ircd::m::dbs::room_head)
ircd::m::dbs::room_head;

decltype(ircd::m::dbs::desc::room_head__comp)
ircd::m::dbs::desc::room_head__comp
{
	{ "name",     "ircd.m.dbs._room_head.comp" },
	{ "default",  string_view{}                },
};

decltype(ircd::m::dbs::desc::room_head__block__size)
ircd::m::dbs::desc::room_head__block__size
{
	{ "name",     "ircd.m.dbs._room_head.block.size" },
	{ "default",  long(4_KiB)                        },
};

decltype(ircd::m::dbs::desc::room_head__meta_block__size)
ircd::m::dbs::desc::room_head__meta_block__size
{
	{ "name",     "ircd.m.dbs._room_head.meta_block.size" },
	{ "default",  long(4_KiB)                             },
};

decltype(ircd::m::dbs::desc::room_head__cache__size)
ircd::m::dbs::desc::room_head__cache__size
{
	{
		{ "name",     "ircd.m.dbs._room_head.cache.size" },
		{ "default",  long(8_MiB)                        },
	}, []
	{
		const size_t &value{room_head__cache__size};
		db::capacity(db::cache(dbs::room_head), value);
	}
};

/// prefix transform for room_id,event_id in room_id
///
const ircd::db::prefix_transform
ircd::m::dbs::desc::room_head__pfx
{
	"_room_head",

	[](const string_view &key)
	{
		return has(key, "\0"_sv);
	},

	[](const string_view &key)
	{
		return split(key, '\0').first;
	}
};

/// This column stores unreferenced (head) events for a room.
///
const ircd::db::descriptor
ircd::m::dbs::desc::room_head
{
	// name
	"_room_head",

	// explanation
	R"(Unreferenced events in a room.

	[room_id | event_id => event_idx]

	The key is a room_id and event_id concatenation. The value is an event_idx
	of the event_id in the key. The key amalgam was specifically selected to
	allow for DELETES sent to the WAL "in the blind" for all prev_events when
	any new event is saved to the database, without making any read IO's to
	look up anything about the prev reference to remove.

	This is a fast-moving column where unreferenced events are inserted and
	then deleted the first time another event is seen which references it so
	it collects a lot of DELETE commands in the WAL and has to be compacted
	often to reduce them out.

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
	room_head__pfx,

	// drop column
	false,

	// cache size
	bool(cache_enable)? -1 : 0,

	// cache size for compressed assets
	0, //no compresed cache

	// bloom filter bits
	0, //table too ephemeral for bloom generation/usefulness

	// expect queries hit
	false,

	// block size
	size_t(room_head__block__size),

	// meta_block size
	size_t(room_head__meta_block__size),

	// compression
	string_view{room_head__comp},

	// compactor
	{},

	// compaction priority algorithm
	"kByCompensatedSize"s,

	// target file size
	{},

	// max bytes for each level
	{},

	// compaction_period
	60s * 60 * 24 * 1     // compact the room head every day.
};

//
// indexer
//

void
ircd::m::dbs::_index_room_head(db::txn &txn,
                               const event &event,
                               const write_opts &opts)
{
	assert(opts.appendix.test(appendix::ROOM_HEAD));
	assert(opts.event_idx);
	assert(event.event_id);

	const ctx::critical_assertion ca;
	thread_local char buf[ROOM_HEAD_KEY_MAX_SIZE];
	const string_view &key
	{
		room_head_key(buf, at<"room_id"_>(event), event.event_id)
	};

	db::txn::append
	{
		txn, room_head,
		{
			opts.op,
			key,
			byte_view<string_view>{opts.event_idx}
		}
	};
}

void
ircd::m::dbs::_index_room_head_resolve(db::txn &txn,
                                       const event &event,
                                       const write_opts &opts)
{
	assert(opts.appendix.test(appendix::ROOM_HEAD_RESOLVE));

	//TODO: If op is DELETE and we are deleting this event and thereby
	//TODO: potentially creating a gap in the reference graph (just for us
	//TODO: though) can we *re-add* the prev_events to the head?

	if(opts.op != db::op::SET)
		return;

	const event::prev prev{event};
	for(size_t i(0); i < prev.prev_events_count(); ++i)
	{
		const auto &event_id
		{
			prev.prev_event(i)
		};

		thread_local char buf[ROOM_HEAD_KEY_MAX_SIZE];
		const ctx::critical_assertion ca;
		const string_view &key
		{
			room_head_key(buf, at<"room_id"_>(event), event_id)
		};

		db::txn::append
		{
			txn, room_head,
			{
				db::op::DELETE,
				key,
			}
		};
	}
}

//
// key
//

ircd::string_view
ircd::m::dbs::room_head_key(const string_view &amalgam)
{
	const auto &key
	{
		lstrip(amalgam, '\0')
	};

	return
	{
		key
	};
}

ircd::string_view
ircd::m::dbs::room_head_key(const mutable_buffer &out_,
                            const id::room &room_id,
                            const id::event &event_id)
{
	mutable_buffer out{out_};
	consume(out, copy(out, room_id));
	consume(out, copy(out, '\0'));
	consume(out, copy(out, event_id));
	return { data(out_), data(out) };
}
