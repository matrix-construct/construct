// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

/// Residence of the events database instance pointer.
decltype(ircd::m::dbs::events)
ircd::m::dbs::events;

/// Coarse variable for enabling the uncompressed cache on the events database;
/// note this conf item is only effective by setting an environmental variable
/// before daemon startup. It has no effect in any other regard.
decltype(ircd::m::dbs::cache_enable)
ircd::m::dbs::cache_enable
{
	{ "name",     "ircd.m.dbs.cache.enable" },
	{ "default",  true                      },
};

/// Coarse variable for enabling the compressed cache on the events database;
/// note this conf item is only effective by setting an environmental variable
/// before daemon startup. It has no effect in any other regard.
decltype(ircd::m::dbs::cache_comp_enable)
ircd::m::dbs::cache_comp_enable
{
	{ "name",     "ircd.m.dbs.cache.comp.enable" },
	{ "default",  false                          },
};

/// Coarse toggle for the prefetch phase before the transaction building
/// handlers (indexers) are called. If this is false, prefetching will be
/// disabled; otherwise the write_opts passed to write() control whether
/// prefetching is enabled.
decltype(ircd::m::dbs::prefetch_enable)
ircd::m::dbs::prefetch_enable
{
	{ "name",     "ircd.m.dbs.prefetch.enable" },
	{ "default",  true                         },
};

/// The size of the memory buffer for new writes to the DB (backed by the WAL
/// on disk). When this buffer is full it is flushed to sorted SST files on
/// disk. If this is 0, a per-column value can be used; otherwise this value
/// takes precedence as a total value for all columns. (db_write_buffer_size)
decltype(ircd::m::dbs::mem_write_buffer_size)
ircd::m::dbs::mem_write_buffer_size
{
	{ "name",     "ircd.m.dbs.mem.write_buffer_size" },
	{ "default",  0L                                 },
};

/// Value determines the size of writes when creating SST files (i.e during
/// compaction). Consider that write calls are yield-points for IRCd and the
/// time spent filling the write buffer between calls may hog the CPU doing
/// compression during that time etc. (writable_file_max_buffer_size)
decltype(ircd::m::dbs::sst_write_buffer_size)
ircd::m::dbs::sst_write_buffer_size
{
	{
		{ "name",     "ircd.m.dbs.sst.write_buffer_size" },
		{ "default",  long(1_MiB)                        },
	}, []
	{
		static const string_view key{"writable_file_max_buffer_size"};
		const size_t &value{sst_write_buffer_size};
		if(events && !events->slave)
			db::setopt(*events, key, lex_cast(value));
	}
};

//
// init
//

/// Initializes the m::dbs subsystem; sets up the events database. Held/called
/// by m::init. Most of the extern variables in m::dbs are not ready until
/// this call completes.
///
/// We also update the fs::basepath for the database directory to include our
/// servername in the path component. The fs::base::DB setting was generated
/// during the build and install process, and is unaware of our servername
/// at runtime. This change deconflicts multiple instances of IRCd running in
/// the same installation prefix using different servernames (i.e clustering
/// on the same machine).
///
ircd::m::dbs::init::init(const string_view &servername,
                         std::string dbopts)
:our_dbpath
{
	fs::path_string(fs::path_views
	{
		fs::base::db, servername
	})
}
,their_dbpath
{
	fs::base::db
}
{
	// NOTE that this is a global change that leaks outside of ircd::m. The
	// database directory for the entire process is being changed here.
	fs::base::db.set(our_dbpath);

	// Recall the db directory init manually with the now-updated basepath
	db::init::directory();

	// Open the events database
	static const string_view &dbname{"events"};
	events = std::make_shared<database>(dbname, std::move(dbopts), desc::events);

	// Cache the columns for the event tuple in order for constant time lookup
	assert(event_columns == event::size());
	std::array<string_view, event::size()> keys;      //TODO: why did this happen?
	_key_transform(event{}, begin(keys), end(keys));  //TODO: how did this happen?

	// Construct global convenience references for the event property columns.
	for(size_t i(0); i < keys.size(); ++i)
		event_column.at(i) = db::column
		{
			*events, keys.at(i), std::nothrow
		};

	// Construct global convenience references for the metadata columns
	event_idx = db::column{*events, desc::event_idx.name};
	event_json = db::column{*events, desc::event_json.name};
	event_refs = db::domain{*events, desc::event_refs.name};
	event_horizon = db::domain{*events, desc::event_horizon.name};
	event_sender = db::domain{*events, desc::event_sender.name};
	event_type = db::domain{*events, desc::event_type.name};
	event_state = db::domain{*events, desc::event_state.name};
	room_head = db::domain{*events, desc::room_head.name};
	room_events = db::domain{*events, desc::room_events.name};
	room_type = db::domain{*events, desc::room_type.name};
	room_joined = db::domain{*events, desc::room_joined.name};
	room_state = db::domain{*events, desc::room_state.name};
	room_state_space = db::domain{*events, desc::room_state_space.name};
}

/// Shuts down the m::dbs subsystem; closes the events database. The extern
/// variables in m::dbs will no longer be functioning after this call.
ircd::m::dbs::init::~init()
noexcept
{
	// Unref DB (should close)
	events = {};

	// restore the fs::base::DB path the way we found it.
	fs::base::db.set(their_dbpath);
}

/// Cancels all background work by the events database. This will make the
/// database shutdown more fluid, without waiting for large compactions.
static const ircd::run::changed
ircd_m_dbs_handle_quit
{
	ircd::run::level::QUIT, []
	{
		if(ircd::m::dbs::events)
			ircd::db::bgcancel(*ircd::m::dbs::events, false); // non-blocking
	}
};

//
// write_opts
//

decltype(ircd::m::dbs::write_opts::event_refs_all)
ircd::m::dbs::write_opts::event_refs_all{[]
{
	char full[event_refs_all.size()];
	memset(full, '1', sizeof(full));
	return decltype(event_refs_all)
	{
		full, sizeof(full)
	};
}()};

decltype(ircd::m::dbs::write_opts::appendix_all)
ircd::m::dbs::write_opts::appendix_all{[]
{
	char full[appendix_all.size()];
	memset(full, '1', sizeof(full));
	return decltype(appendix_all)
	{
		full, sizeof(full)
	};
}()};

//
// Basic write suite
//

namespace ircd::m::dbs
{
	static size_t _prefetch(const event &, const write_opts &);
	static size_t _index(db::txn &, const event &, const write_opts &);
	static size_t blacklist(db::txn &txn, const event::id &, const write_opts &);
}

size_t
ircd::m::dbs::write(db::txn &txn,
                    const event &event,
                    const write_opts &opts)
try
{
	if(opts.event_idx == 0 && opts.blacklist)
		return blacklist(txn, event.event_id, opts);

	if(unlikely(opts.event_idx == 0))
		throw panic
		{
			"Cannot write to database: no index specified for event."
		};

	return _index(txn, event, opts);
}
catch(const std::exception &e)
{
	log::error
	{
		log, "Event %s txn building error :%s",
		string_view{event.event_id},
		e.what()
	};

	throw;
}

size_t
ircd::m::dbs::prefetch(const event &event,
                       const write_opts &opts)
try
{
	if(!prefetch_enable)
		return false;

	return _prefetch(event, opts);
}
catch(const std::exception &e)
{
	log::error
	{
		log, "Event %s txn prefetching error :%s",
		string_view{event.event_id},
		e.what()
	};

	return false;
}

size_t
ircd::m::dbs::blacklist(db::txn &txn,
                        const event::id &event_id,
                        const write_opts &opts)
{
	// An entry in the event_idx column with a value 0 is blacklisting
	// because 0 is not a valid event_idx. Thus a value here can only
	// have the value zero.
	assert(opts.event_idx == 0);
	assert(!event_id.empty());

	static const m::event::idx &zero_idx{0UL};
	static const byte_view<string_view> zero_value
	{
		zero_idx
	};

	db::txn::append
	{
		txn, event_idx,
		{
			opts.op,
			string_view{event_id},
			zero_value
		}
	};

	return true;
}

//
// Internal interface
//

namespace ircd::m::dbs
{
	static size_t _prefetch_room_redact(const event &, const write_opts &);
	static void _index_room_redact(db::txn &, const event &, const write_opts &);

	static size_t _prefetch_room(const event &, const write_opts &);
	static void _index_room(db::txn &, const event &, const write_opts &);

	static size_t _prefetch_event(const event &, const write_opts &);
	static void _index_event(db::txn &, const event &, const write_opts &);
}

size_t
ircd::m::dbs::_index(db::txn &txn,
                     const event &event,
                     const write_opts &opts)
{
	size_t ret(0);
	_index_event(txn, event, opts);

	if(json::get<"room_id"_>(event))
		_index_room(txn, event, opts);

	return ret;
}

size_t
ircd::m::dbs::_prefetch(const event &event,
                        const write_opts &opts)
{
	size_t ret(0);
	ret += _prefetch_event(event, opts);

	if(json::get<"room_id"_>(event))
		ret += _prefetch_room(event, opts);

	return ret;
}

void
ircd::m::dbs::_index_event(db::txn &txn,
                           const event &event,
                           const write_opts &opts)
{
	if(opts.appendix.test(appendix::EVENT_ID))
		_index_event_id(txn, event, opts);

	if(opts.appendix.test(appendix::EVENT_COLS))
		_index_event_cols(txn, event, opts);

	if(opts.appendix.test(appendix::EVENT_JSON))
		_index_event_json(txn, event, opts);

	if(opts.appendix.test(appendix::EVENT_SENDER))
		_index_event_sender(txn, event, opts);

	if(opts.appendix.test(appendix::EVENT_TYPE))
		_index_event_type(txn, event, opts);

	if(opts.appendix.test(appendix::EVENT_STATE))
		_index_event_state(txn, event, opts);

	if(opts.appendix.test(appendix::EVENT_REFS) && opts.event_refs.any())
		_index_event_refs(txn, event, opts);

	if(opts.appendix.test(appendix::EVENT_HORIZON_RESOLVE) && opts.horizon_resolve.any())
		_index_event_horizon_resolve(txn, event, opts);
}

size_t
ircd::m::dbs::_prefetch_event(const event &event,
                              const write_opts &opts)
{
	size_t ret(0);
	if(opts.appendix.test(appendix::EVENT_ID))
		;//ret += _prefetch_event_id(txn, event, opts);

	if(opts.appendix.test(appendix::EVENT_COLS))
		;//ret += _prefetch_event_cols(txn, event, opts);

	if(opts.appendix.test(appendix::EVENT_JSON))
		;//ret += _prefetch_event_json(txn, event, opts);

	if(opts.appendix.test(appendix::EVENT_SENDER))
		;//ret += _prefetch_event_sender(txn, event, opts);

	if(opts.appendix.test(appendix::EVENT_TYPE))
		;//ret += _prefetch_event_type(txn, event, opts);

	if(opts.appendix.test(appendix::EVENT_STATE))
		;//ret += _prefetch_event_state(txn, event, opts);

	if(opts.appendix.test(appendix::EVENT_REFS) && opts.event_refs.any())
		ret += _prefetch_event_refs(event, opts);

	if(opts.appendix.test(appendix::EVENT_HORIZON_RESOLVE) && opts.horizon_resolve.any())
		ret += _prefetch_event_horizon_resolve(event, opts);

	return ret;
}

void
ircd::m::dbs::_index_room(db::txn &txn,
                          const event &event,
                          const write_opts &opts)
{
	assert(!empty(json::get<"room_id"_>(event)));

	if(opts.appendix.test(appendix::ROOM_EVENTS))
		_index_room_events(txn, event, opts);

	if(opts.appendix.test(appendix::ROOM_TYPE))
		_index_room_type(txn, event, opts);

	if(opts.appendix.test(appendix::ROOM_HEAD))
		_index_room_head(txn, event, opts);

	if(opts.appendix.test(appendix::ROOM_HEAD_RESOLVE))
		_index_room_head_resolve(txn, event, opts);

	if(defined(json::get<"state_key"_>(event)))
	{
		if(opts.appendix.test(appendix::ROOM_STATE))
			_index_room_state(txn, event, opts);

		if(opts.appendix.test(appendix::ROOM_STATE_SPACE))
			_index_room_state_space(txn, event, opts);

		if(opts.appendix.test(appendix::ROOM_JOINED) && at<"type"_>(event) == "m.room.member")
			_index_room_joined(txn, event, opts);
	}

	if(opts.appendix.test(appendix::ROOM_REDACT) && json::get<"type"_>(event) == "m.room.redaction")
		_index_room_redact(txn, event, opts);
}

size_t
ircd::m::dbs::_prefetch_room(const event &event,
                             const write_opts &opts)
{
	assert(!empty(json::get<"room_id"_>(event)));

	size_t ret(0);
	if(opts.appendix.test(appendix::ROOM_EVENTS))
		;//ret += _prefetch_room_events(event, opts);

	if(opts.appendix.test(appendix::ROOM_TYPE))
		;//ret += _prefetch_room_type(event, opts);

	if(opts.appendix.test(appendix::ROOM_HEAD))
		;//ret += _prefetch_room_head(event, opts);

	if(opts.appendix.test(appendix::ROOM_HEAD_RESOLVE))
		;//ret += _prefetch_room_head_resolve(event, opts);

	if(defined(json::get<"state_key"_>(event)))
	{
		if(opts.appendix.test(appendix::ROOM_STATE))
			;//ret += _prefetch_room_state(event, opts);

		if(opts.appendix.test(appendix::ROOM_STATE_SPACE))
			;//ret += _prefetch_room_state_space(event, opts);

		if(opts.appendix.test(appendix::ROOM_JOINED) && at<"type"_>(event) == "m.room.member")
			;//ret += _prefetch_room_joined(event, opts);
	}

	if(opts.appendix.test(appendix::ROOM_REDACT) && json::get<"type"_>(event) == "m.room.redaction")
		ret += _prefetch_room_redact(event, opts);

	return ret;
}

// NOTE: QUERY
void
ircd::m::dbs::_index_room_redact(db::txn &txn,
                                 const event &event,
                                 const write_opts &opts)
{
	assert(opts.appendix.test(appendix::ROOM_REDACT));
	assert(json::get<"type"_>(event) == "m.room.redaction");

	const auto &target_id
	{
		at<"redacts"_>(event)
	};

	const m::event::idx target_idx
	{
		find_event_idx(target_id, opts)
	};

	if(unlikely(!target_idx))
	{
		log::dwarning
		{
			"Redaction from '%s' missing redaction target '%s'",
			string_view{event.event_id},
			target_id
		};

		if(opts.appendix.test(appendix::EVENT_HORIZON))
			_index_event_horizon(txn, event, opts, target_id);

		return;
	}

	char state_key_buf[event::STATE_KEY_MAX_SIZE];
	const string_view &state_key
	{
		m::get(std::nothrow, target_idx, "state_key", state_key_buf)
	};

	if(!state_key)
		return;

	char type_buf[event::TYPE_MAX_SIZE];
	const string_view &type
	{
		m::get(std::nothrow, target_idx, "type", type_buf)
	};

	assert(!empty(type));
	const ctx::critical_assertion ca;
	thread_local char buf[ROOM_STATE_SPACE_KEY_MAX_SIZE];
	const string_view &key
	{
		room_state_key(buf, at<"room_id"_>(event), type, state_key)
	};

	db::txn::append
	{
		txn, room_state,
		{
			db::op::DELETE,
			key,
		}
	};
}

size_t
ircd::m::dbs::_prefetch_room_redact(const event &event,
                                    const write_opts &opts)
{
	assert(opts.appendix.test(appendix::ROOM_REDACT));
	assert(json::get<"type"_>(event) == "m.room.redaction");

	const auto &target_id
	{
		at<"redacts"_>(event)
	};

	// If the prefetch was launched we can't do anything more here.
	if(prefetch_event_idx(target_id, opts))
		return 1;

	// If the result is cached we can peek at it for more prefetches.
	const m::event::idx target_idx
	{
		find_event_idx(target_id, opts)
	};

	if(unlikely(!target_idx))
		return 0;

	size_t ret(0);
	ret += m::prefetch(target_idx, "state_key");
	ret += m::prefetch(target_idx, "type");
	return ret;
}

// NOTE: QUERY
size_t
ircd::m::dbs::find_event_idx(const vector_view<event::idx> &idx,
                             const vector_view<const event::id> &event_id,
                             const write_opts &wopts)
{
	const size_t num
	{
		std::min(idx.size(), event_id.size())
	};

	size_t ret(0);
	if(wopts.interpose)
		for(size_t i(0); i < num; ++i)
		{
			idx[i] = wopts.interpose->val(db::op::SET, "_event_idx", event_id[i], 0UL);
			assert(!idx[i] || idx[i] >= vm::sequence::retired);
			ret += idx[i] != 0;
		}

	// Taken when everything satisfied by interpose
	if(ret == num || !wopts.allow_queries)
		return ret;

	// Only do parallel m::index() if there's no results from the prior
	// queries; they'll get clobbered by the parallel m::index().
	if(likely(!ret))
		return m::index(idx, event_id);

	// Fallback to serial queries.
	for(size_t i(0); i < num; ++i)
	{
		idx[i] = m::index(std::nothrow, event_id[i]);
		ret += idx[i] != 0;
	}

	return ret;
}

size_t
ircd::m::dbs::prefetch_event_idx(const vector_view<const event::id> &event_id,
                                 const write_opts &wopts)
{
	size_t ret(0);
	for(size_t i(0); i < event_id.size(); ++i)
	{
		if(wopts.interpose)
			if(wopts.interpose->has(db::op::SET, "_event_idx", event_id[i]))
				continue;

		if(wopts.allow_queries)
			ret += m::prefetch(event_id[i], "_event_idx");
	}

	return ret;
}
