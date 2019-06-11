// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

/// Residence of the events database instance pointer.
decltype(ircd::m::dbs::events)
ircd::m::dbs::events
{};

/// Linkage for a cache of the columns of the events database which directly
/// correspond to a property in the matrix event object. This array allows
/// for constant time access to a column the same way one can make constant
/// time access to a property in m::event.
decltype(ircd::m::dbs::event_column)
ircd::m::dbs::event_column
{};

/// Linkage for a reference to the event_seq column.
decltype(ircd::m::dbs::event_idx)
ircd::m::dbs::event_idx
{};

/// Linkage for a reference to the event_json column.
decltype(ircd::m::dbs::event_json)
ircd::m::dbs::event_json
{};

/// Linkage for a reference to the event_refs column.
decltype(ircd::m::dbs::event_refs)
ircd::m::dbs::event_refs
{};

/// Linkage for a reference to the event_horizon column.
decltype(ircd::m::dbs::event_horizon)
ircd::m::dbs::event_horizon
{};

/// Linkage for a reference to the event_sender column.
decltype(ircd::m::dbs::event_sender)
ircd::m::dbs::event_sender
{};

/// Linkage for a reference to the event_type column.
decltype(ircd::m::dbs::event_type)
ircd::m::dbs::event_type
{};

/// Linkage for a reference to the room_head column
decltype(ircd::m::dbs::room_head)
ircd::m::dbs::room_head
{};

/// Linkage for a reference to the room_events column
decltype(ircd::m::dbs::room_events)
ircd::m::dbs::room_events
{};

/// Linkage for a reference to the room_joined column
decltype(ircd::m::dbs::room_joined)
ircd::m::dbs::room_joined
{};

/// Linkage for a reference to the room_state column
decltype(ircd::m::dbs::room_state)
ircd::m::dbs::room_state
{};

/// Linkage for a reference to the room_state_space column
decltype(ircd::m::dbs::room_state_space)
ircd::m::dbs::room_state_space
{};

/// Linkage for a reference to the state_node column.
decltype(ircd::m::dbs::state_node)
ircd::m::dbs::state_node
{};

/// Coarse variable for enabling the uncompressed cache on the events database;
/// note this conf item is only effective by setting an environmental variable
/// before daemon startup. It has no effect in any other regard.
decltype(ircd::m::dbs::events_cache_enable)
ircd::m::dbs::events_cache_enable
{
	{ "name",     "ircd.m.dbs.events.__cache_enable" },
	{ "default",  true                               },
};

/// Coarse variable for enabling the compressed cache on the events database;
/// note this conf item is only effective by setting an environmental variable
/// before daemon startup. It has no effect in any other regard.
decltype(ircd::m::dbs::events_cache_comp_enable)
ircd::m::dbs::events_cache_comp_enable
{
	{ "name",     "ircd.m.dbs.events.__cache_comp_enable" },
	{ "default",  false                                   },
};

/// Value determines the size of writes when creating SST files (i.e during
/// compaction). Consider that write calls are yield-points for IRCd and the
/// time spent filling the write buffer between calls may hog the CPU doing
/// compression during that time etc. (writable_file_max_buffer_size)
decltype(ircd::m::dbs::events_sst_write_buffer_size)
ircd::m::dbs::events_sst_write_buffer_size
{
	{
		{ "name",     "ircd.m.dbs.events.sst.write_buffer_size" },
		{ "default",  long(1_MiB)                               },
	}, []
	{
		static const auto key{"writable_file_max_buffer_size"_sv};
		const size_t &value{events_sst_write_buffer_size};
		if(events)
			db::setopt(*events, key, lex_cast(value));
	}
};

/// The size of the memory buffer for new writes to the DB (backed by the WAL
/// on disk). When this buffer is full it is flushed to sorted SST files on
/// disk. If this is 0, a per-column value can be used; otherwise this value
/// takes precedence as a total value for all columns. (db_write_buffer_size)
decltype(ircd::m::dbs::events_mem_write_buffer_size)
ircd::m::dbs::events_mem_write_buffer_size
{
	{ "name",     "ircd.m.dbs.events.mem.write_buffer_size" },
	{ "default",  0L                                        },
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
	ircd::string(fs::PATH_MAX_LEN | SHRINK_TO_FIT, [&servername]
	(const mutable_buffer &buf)
	{
		return fs::path(buf, fs::base::DB, servername);
	})
}
,their_dbpath
{
	// NOTE that this is a global change that leaks outside of ircd::m. The
	// database directory for the entire process is being changed here.
	fs::basepath::set(fs::base::DB, our_dbpath)
}
{
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
	event_idx = db::column{*events, desc::events__event_idx.name};
	event_json = db::column{*events, desc::events__event_json.name};
	event_refs = db::domain{*events, desc::events__event_refs.name};
	event_horizon = db::domain{*events, desc::events__event_horizon.name};
	event_sender = db::domain{*events, desc::events__event_sender.name};
	event_type = db::domain{*events, desc::events__event_type.name};
	room_head = db::domain{*events, desc::events__room_head.name};
	room_events = db::domain{*events, desc::events__room_events.name};
	room_joined = db::domain{*events, desc::events__room_joined.name};
	room_state = db::domain{*events, desc::events__room_state.name};
	room_state_space = db::domain{*events, desc::events__room_state_space.name};
	state_node = db::column{*events, desc::events__state_node.name};
}

/// Shuts down the m::dbs subsystem; closes the events database. The extern
/// variables in m::dbs will no longer be functioning after this call.
ircd::m::dbs::init::~init()
noexcept
{
	// Unref DB (should close)
	events = {};

	// restore the fs::base::DB path the way we found it.
	fs::basepath::set(fs::base::DB, their_dbpath);
}

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
	static void _index_room_joined(db::txn &, const event &, const write_opts &);
	static void _index_room_redact(db::txn &, const event &, const write_opts &);
	static void _index_room_state_space(db::txn &,  const event &, const write_opts &);
	static void _index_room_state(db::txn &, const event &, const write_opts &);
	static void _index_room_head_resolve(db::txn &, const event &, const write_opts &);
	static void _index_room_head(db::txn &, const event &, const write_opts &);
	static void _index_room_events(db::txn &,  const event &, const write_opts &);
	static void _index_room(db::txn &, const event &, const write_opts &);
	static void _index_event_type(db::txn &, const event &, const write_opts &);
	static void _index_event_sender(db::txn &, const event &, const write_opts &);
	static void _index_event_horizon_resolve(db::txn &, const event &, const write_opts &);
	static void _index_event_horizon(db::txn &, const event &, const write_opts &, const id::event &);
	static void _index_event_refs_m_room_redaction(db::txn &, const event &, const write_opts &);
	static void _index_event_refs_m_receipt_m_read(db::txn &, const event &, const write_opts &);
	static void _index_event_refs_m_relates_m_reply(db::txn &, const event &, const write_opts &);
	static void _index_event_refs_state(db::txn &, const event &, const write_opts &);
	static void _index_event_refs_auth(db::txn &, const event &, const write_opts &);
	static void _index_event_refs_prev(db::txn &, const event &, const write_opts &);
	static void _index_event_refs(db::txn &, const event &, const write_opts &);
	static void _index_event_json(db::txn &, const event &, const write_opts &);
	static void _index_event_cols(db::txn &, const event &, const write_opts &);
	static void _index_event_id(db::txn &, const event &, const write_opts &);
	static void _index_event(db::txn &, const event &, const write_opts &);

	static void blacklist(db::txn &txn, const event::id &, const write_opts &);
}

void
ircd::m::dbs::write(db::txn &txn,
                    const event &event,
                    const write_opts &opts)
try
{
	if(opts.event_idx == 0 && opts.blacklist)
		return blacklist(txn, at<"event_id"_>(event), opts);

	if(unlikely(opts.event_idx == 0))
		throw panic
		{
			"Cannot write to database: no index specified for event."
		};

	_index_event(txn, event, opts);
	if(json::get<"room_id"_>(event))
		_index_room(txn, event, opts);
}
catch(const std::exception &e)
{
	log::error
	{
		log, "Event %s txn building error :%s",
		json::get<"event_id"_>(event),
		e.what()
	};

	throw;
}

void
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
}

//
// Internal interface
//

namespace ircd::m::dbs
{
	static event::idx find_event_idx(const event::id &, const write_opts &);
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

	if(opts.appendix.test(appendix::EVENT_REFS) && opts.event_refs.any())
		_index_event_refs(txn, event, opts);

	if(opts.appendix.test(appendix::EVENT_HORIZON_RESOLVE) && opts.horizon_resolve.any())
		_index_event_horizon_resolve(txn, event, opts);
}

void
ircd::m::dbs::_index_event_id(db::txn &txn,
                              const event &event,
                              const write_opts &opts)
{
	assert(opts.appendix.test(appendix::EVENT_ID));
	assert(opts.event_idx);

	db::txn::append
	{
		txn, dbs::event_idx,
		{
			opts.op,
			at<"event_id"_>(event),
			byte_view<string_view>(opts.event_idx)
		}
	};
}

void
ircd::m::dbs::_index_event_cols(db::txn &txn,
                                const event &event,
                                const write_opts &opts)
{
	assert(opts.appendix.test(appendix::EVENT_COLS));
	assert(opts.event_idx);
	const byte_view<string_view> key
	{
		opts.event_idx
	};

	size_t i{0};
	for_each(event, [&txn, &opts, &key, &i]
	(const auto &, auto&& val)
	{
		auto &column
		{
			event_column.at(i++)
		};

		if(!column)
			return;

		if(value_required(opts.op) && !defined(json::value(val)))
			return;

		db::txn::append
		{
			txn, column, db::column::delta
			{
				opts.op,
				string_view{key},
				value_required(opts.op)?
					byte_view<string_view>{val}:
					byte_view<string_view>{}
			}
		};
	});
}

void
ircd::m::dbs::_index_event_json(db::txn &txn,
                                const event &event,
                                const write_opts &opts)
{
	const ctx::critical_assertion ca;
	thread_local char buf[m::event::MAX_SIZE];
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
			json::stringify(mutable_buffer{buf}, event.source):

		// If no source was given with the event we can generate it.
		opts.op == db::op::SET?
			json::stringify(mutable_buffer{buf}, event):

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

void
ircd::m::dbs::_index_event_refs(db::txn &txn,
                                const event &event,
                                const write_opts &opts)
{
	assert(opts.appendix.test(appendix::EVENT_REFS));

	if(opts.event_refs.test(uint(ref::NEXT)))
		_index_event_refs_prev(txn, event, opts);

	if(opts.event_refs.test(uint(ref::NEXT_AUTH)))
		_index_event_refs_auth(txn, event, opts);

	if(opts.event_refs.test(uint(ref::NEXT_STATE)) ||
	   opts.event_refs.test(uint(ref::PREV_STATE)))
		_index_event_refs_state(txn, event, opts);

	if(opts.event_refs.test(uint(ref::M_RECEIPT__M_READ)))
		_index_event_refs_m_receipt_m_read(txn, event, opts);

	if(opts.event_refs.test(uint(ref::M_RELATES__M_REPLY)))
		_index_event_refs_m_relates_m_reply(txn, event, opts);

	if(opts.event_refs.test(uint(ref::M_ROOM_REDACTION)))
		_index_event_refs_m_room_redaction(txn, event, opts);
}

void
ircd::m::dbs::_index_event_refs_prev(db::txn &txn,
                                     const event &event,
                                     const write_opts &opts)
{
	assert(opts.appendix.test(appendix::EVENT_REFS));
	assert(opts.event_refs.test(uint(ref::NEXT)));

	const event::prev &prev{event};
	for(size_t i(0); i < prev.prev_events_count(); ++i)
	{
		const event::id &prev_id
		{
			prev.prev_event(i)
		};

		const event::idx &prev_idx
		{
			find_event_idx(prev_id, opts)
		};

		if(opts.appendix.test(appendix::EVENT_HORIZON) && !prev_idx)
		{
			_index_event_horizon(txn, event, opts, prev_id);
			continue;
		}
		else if(!prev_idx)
		{
			log::derror
			{
				log, "No index found to ref %s PREV of %s",
				string_view{prev_id},
				json::get<"event_id"_>(event),
			};

			continue;
		}

		thread_local char buf[EVENT_REFS_KEY_MAX_SIZE];
		assert(opts.event_idx != 0 && prev_idx != 0);
		assert(opts.event_idx != prev_idx);
		const string_view &key
		{
			event_refs_key(buf, prev_idx, ref::NEXT, opts.event_idx)
		};

		db::txn::append
		{
			txn, dbs::event_refs,
			{
				opts.op, key
			}
		};
	}
}

void
ircd::m::dbs::_index_event_refs_auth(db::txn &txn,
                                     const event &event,
                                     const write_opts &opts)
{
	assert(opts.appendix.test(appendix::EVENT_REFS));
	assert(opts.event_refs.test(uint(ref::NEXT_AUTH)));
	if(!event::auth::is_power_event(event))
		return;

	const event::prev &prev{event};
	for(size_t i(0); i < prev.auth_events_count(); ++i)
	{
		const event::id &auth_id
		{
			prev.auth_event(i)
		};

		const event::idx &auth_idx
		{
			find_event_idx(auth_id, opts)
		};

		if(unlikely(!auth_idx))
		{
			if(opts.appendix.test(appendix::EVENT_HORIZON))
				_index_event_horizon(txn, event, opts, auth_id);

			log::error
			{
				log, "No index found to ref %s AUTH of %s",
				string_view{auth_id},
				json::get<"event_id"_>(event)
			};

			continue;
		}

		thread_local char buf[EVENT_REFS_KEY_MAX_SIZE];
		assert(opts.event_idx != 0 && auth_idx != 0);
		assert(opts.event_idx != auth_idx);
		const string_view &key
		{
			event_refs_key(buf, auth_idx, ref::NEXT_AUTH, opts.event_idx)
		};

		db::txn::append
		{
			txn, dbs::event_refs,
			{
				opts.op, key
			}
		};
	}
}

void
ircd::m::dbs::_index_event_refs_state(db::txn &txn,
                                      const event &event,
                                      const write_opts &opts)
{
	assert(opts.appendix.test(appendix::EVENT_REFS));
	assert(opts.event_refs.test(uint(ref::NEXT_STATE)) ||
	       opts.event_refs.test(uint(ref::PREV_STATE)));

	if(!json::get<"room_id"_>(event))
		return;

	if(!json::get<"state_key"_>(event))
		return;

	const m::room room
	{
		at<"room_id"_>(event) //TODO: ABA ABA ABA ABA
	};

	const m::room::state state
	{
		room
	};

	const event::idx &prev_state_idx
	{
		opts.allow_queries?
			state.get(std::nothrow, at<"type"_>(event), at<"state_key"_>(event)): // query
			0UL
	};

	// No previous state; nothing to do.
	if(!prev_state_idx)
		return;

	// If the previous state's event_idx is greater than the event_idx of the
	// event we're transacting this is almost surely a replay/rewrite. Bail
	// out for now rather than corrupting the graph.
	if(unlikely(prev_state_idx >= opts.event_idx))
		return;

	thread_local char buf[EVENT_REFS_KEY_MAX_SIZE];
	assert(opts.event_idx != 0 && prev_state_idx != 0);
	assert(opts.event_idx != prev_state_idx);

	if(opts.event_refs.test(uint(ref::NEXT_STATE)))
	{
		const string_view &key
		{
			event_refs_key(buf, prev_state_idx, ref::NEXT_STATE, opts.event_idx)
		};

		db::txn::append
		{
			txn, dbs::event_refs,
			{
				opts.op, key
			}
		};
	}

	if(opts.event_refs.test(uint(ref::PREV_STATE)))
	{
		const string_view &key
		{
			event_refs_key(buf, opts.event_idx, ref::PREV_STATE, prev_state_idx)
		};

		db::txn::append
		{
			txn, dbs::event_refs,
			{
				opts.op, key
			}
		};
	}
}

void
ircd::m::dbs::_index_event_refs_m_receipt_m_read(db::txn &txn,
                                                 const event &event,
                                                 const write_opts &opts)
{
	assert(opts.appendix.test(appendix::EVENT_REFS));
	assert(opts.event_refs.test(uint(ref::M_RECEIPT__M_READ)));

	if(json::get<"type"_>(event) != "ircd.read")
		return;

	if(!my_host(json::get<"origin"_>(event)))
		return;

	//TODO: disallow local forge?

	const json::string &event_id
	{
		json::get<"content"_>(event).get("event_id")
	};

	const event::idx &event_idx
	{
		find_event_idx(event_id, opts)
	};

	if(opts.appendix.test(appendix::EVENT_HORIZON) && !event_idx)
	{
		_index_event_horizon(txn, event, opts, event_id);
		return;
	}
	else if(!event_idx)
	{
		log::derror
		{
			log, "No index found to ref %s M_RECEIPT__M_READ of %s",
			string_view{event_id},
			json::get<"event_id"_>(event)
		};

		return;
	}

	thread_local char buf[EVENT_REFS_KEY_MAX_SIZE];
	assert(opts.event_idx != 0 && event_idx != 0);
	assert(opts.event_idx != event_idx);
	const string_view &key
	{
		event_refs_key(buf, event_idx, ref::M_RECEIPT__M_READ, opts.event_idx)
	};

	db::txn::append
	{
		txn, dbs::event_refs,
		{
			opts.op, key
		}
	};
}

void
ircd::m::dbs::_index_event_refs_m_relates_m_reply(db::txn &txn,
                                                  const event &event,
                                                  const write_opts &opts)
{
	assert(opts.appendix.test(appendix::EVENT_REFS));
	assert(opts.event_refs.test(uint(ref::M_RELATES__M_REPLY)));

	if(json::get<"type"_>(event) != "m.room.message")
		return;

	if(!json::get<"content"_>(event).has("m.relates_to"))
		return;

	if(json::type(json::get<"content"_>(event).get("m.relates_to")) != json::OBJECT)
		return;

	const json::object &m_relates_to
	{
		json::get<"content"_>(event).get("m.relates_to")
	};

	if(!m_relates_to.has("m.in_reply_to"))
		return;

	if(json::type(m_relates_to.get("m.in_reply_to")) != json::OBJECT)
	{
		log::derror
		{
			log, "Cannot index m.in_reply_to in %s; not an OBJECT.",
			json::get<"event_id"_>(event)
		};

		return;
	}

	const json::object &m_in_reply_to
	{
		m_relates_to.get("m.in_reply_to")
	};

	const json::string &event_id
	{
		m_in_reply_to.get("event_id")
	};

	if(!valid(m::id::EVENT, event_id))
	{
		log::derror
		{
			log, "Cannot index m.in_reply_to in %s; '%s' is not an event_id.",
			json::get<"event_id"_>(event),
			string_view{event_id}
		};

		return;
	}

	const event::idx &event_idx
	{
		find_event_idx(event_id, opts)
	};

	if(opts.appendix.test(appendix::EVENT_HORIZON) && !event_idx)
	{
		_index_event_horizon(txn, event, opts, event_id);
		return;
	}
	else if(!event_idx)
	{
		log::derror
		{
			log, "Cannot index m.in_reply_to in %s; referenced %s not found.",
			json::get<"event_id"_>(event),
			string_view{event_id}
		};

		return;
	}

	thread_local char buf[EVENT_REFS_KEY_MAX_SIZE];
	assert(opts.event_idx != 0 && event_idx != 0);
	assert(opts.event_idx != event_idx);
	const string_view &key
	{
		event_refs_key(buf, event_idx, ref::M_RELATES__M_REPLY, opts.event_idx)
	};

	db::txn::append
	{
		txn, dbs::event_refs,
		{
			opts.op, key
		}
	};
}

void
ircd::m::dbs::_index_event_refs_m_room_redaction(db::txn &txn,
                                                 const event &event,
                                                 const write_opts &opts)
{
	assert(opts.appendix.test(appendix::EVENT_REFS));
	assert(opts.event_refs.test(uint(ref::M_ROOM_REDACTION)));

	if(json::get<"type"_>(event) != "m.room.redaction")
		return;

	if(!valid(m::id::EVENT, json::get<"redacts"_>(event)))
		return;

	const auto &event_id
	{
		json::get<"redacts"_>(event)
	};

	const event::idx &event_idx
	{
		find_event_idx(event_id, opts)
	};

	if(opts.appendix.test(appendix::EVENT_HORIZON) && !event_idx)
	{
		_index_event_horizon(txn, event, opts, event_id);
		return;
	}
	else if(!event_idx)
	{
		log::derror
		{
			log, "Cannot index m.room.redaction in %s; referenced %s not found.",
			json::get<"event_id"_>(event),
			string_view{event_id}
		};

		return;
	}

	thread_local char buf[EVENT_REFS_KEY_MAX_SIZE];
	assert(opts.event_idx != 0 && event_idx != 0);
	assert(opts.event_idx != event_idx);
	const string_view &key
	{
		event_refs_key(buf, event_idx, ref::M_ROOM_REDACTION, opts.event_idx)
	};

	db::txn::append
	{
		txn, dbs::event_refs,
		{
			opts.op, key
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

void
ircd::m::dbs::_index_event_horizon_resolve(db::txn &txn,
                                           const event &event,
                                           const write_opts &opts)
{
	char buf[EVENT_HORIZON_KEY_MAX_SIZE];
	assert(opts.appendix.test(appendix::EVENT_HORIZON_RESOLVE));
	assert(opts.event_idx != 0);
	const string_view &key
	{
		event_horizon_key(buf, at<"event_id"_>(event))
	};

	auto it
	{
		dbs::event_horizon.begin(key)
	};

	for(; it; ++it)
	{
		const auto parts
		{
			event_horizon_key(it->first)
		};

		const auto &event_idx
		{
			std::get<0>(parts)
		};

		assert(event_idx != 0);
		assert(event_idx != opts.event_idx);
		const event::fetch _event
		{
			event_idx, std::nothrow
		};

		if(!_event.valid)
			continue;

		// Make the references on behalf of the future event
		write_opts _opts;
		_opts.op = opts.op;
		_opts.event_idx = event_idx;
		_opts.appendix.reset();
		_opts.appendix.set(appendix::EVENT_REFS);
		_opts.event_refs = opts.horizon_resolve;
		_opts.interpose = &txn;
		write(txn, _event, _opts);

		// Delete the event_horizon entry after resolving.
		thread_local char buf[EVENT_HORIZON_KEY_MAX_SIZE];
		const string_view &key
		{
			event_horizon_key(buf, at<"event_id"_>(event), event_idx)
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
}

void
ircd::m::dbs::_index_event_sender(db::txn &txn,
                                  const event &event,
                                  const write_opts &opts)
{
	assert(opts.appendix.test(appendix::EVENT_SENDER));
	assert(json::get<"sender"_>(event));
	assert(opts.event_idx);

	thread_local char buf[EVENT_SENDER_KEY_MAX_SIZE];
	const string_view &key
	{
		event_sender_key(buf, at<"sender"_>(event), opts.event_idx)
	};

	db::txn::append
	{
		txn, dbs::event_sender,
		{
			opts.op, key
		}
	};
}

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

void
ircd::m::dbs::_index_room(db::txn &txn,
                          const event &event,
                          const write_opts &opts)
{
	assert(!empty(json::get<"room_id"_>(event)));

	if(opts.appendix.test(appendix::ROOM_EVENTS))
		_index_room_events(txn, event, opts);

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

/// Adds the entry for the room_events column into the txn.
void
ircd::m::dbs::_index_room_events(db::txn &txn,
                                 const event &event,
                                 const write_opts &opts)
{
	assert(opts.appendix.test(appendix::ROOM_EVENTS));

	thread_local char buf[ROOM_EVENTS_KEY_MAX_SIZE];
	const ctx::critical_assertion ca;
	const string_view &key
	{
		room_events_key(buf, at<"room_id"_>(event), at<"depth"_>(event), opts.event_idx)
	};

	db::txn::append
	{
		txn, room_events,
		{
			opts.op,        // db::op
			key,            // key,
		}
	};
}

void
ircd::m::dbs::_index_room_head(db::txn &txn,
                               const event &event,
                               const write_opts &opts)
{
	const ctx::critical_assertion ca;
	thread_local char buf[ROOM_HEAD_KEY_MAX_SIZE];
	assert(opts.appendix.test(appendix::ROOM_HEAD));
	assert(opts.event_idx);

	const string_view &key
	{
		room_head_key(buf, at<"room_id"_>(event), at<"event_id"_>(event))
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

	const m::event::prev &prev{event};
	for(const json::array &p : json::get<"prev_events"_>(prev))
	{
		const auto &event_id
		{
			unquote(p.at(0))
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
		log::error
		{
			"Redaction from '%s' missing redaction target '%s'",
			at<"event_id"_>(event),
			target_id
		};

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

	db::txn::append
	{
		txn, room_joined,
		{
			op,
			key,
		}
	};
}

ircd::m::event::idx
ircd::m::dbs::find_event_idx(const event::id &event_id,
                             const write_opts &wopts)
{
	event::idx ret{0};
	if(wopts.interpose)
		ret = wopts.interpose->val(db::op::SET, "_event_idx", event_id, 0UL);

	if(wopts.allow_queries && !ret)
		ret = m::index(event_id, std::nothrow); // query

	return ret;
}

ircd::string_view
ircd::m::dbs::state_root(const mutable_buffer &out,
                         const event &event)
{
	return state_root(out, at<"room_id"_>(event), at<"event_id"_>(event), at<"depth"_>(event));
}

ircd::string_view
ircd::m::dbs::state_root(const mutable_buffer &out,
                         const id::event &event_id)
{
	return state_root(out, index(event_id));
}

ircd::string_view
ircd::m::dbs::state_root(const mutable_buffer &out,
                         const event::idx &event_idx)
{
	static constexpr auto idx
	{
		json::indexof<event, "room_id"_>()
	};

	auto &column
	{
		event_column.at(idx)
	};

	id::room::buf room_id;
	column(byte_view<string_view>(event_idx), [&room_id]
	(const string_view &val)
	{
		room_id = val;
	});

	return state_root(out, room_id, event_idx);
}

ircd::string_view
ircd::m::dbs::state_root(const mutable_buffer &out,
                         const id::room &room_id,
                         const id::event &event_id)
{
	return state_root(out, room_id, index(event_id));
}

ircd::string_view
ircd::m::dbs::state_root(const mutable_buffer &out,
                         const id::room &room_id,
                         const event::idx &event_idx)
{
	static constexpr auto idx
	{
		json::indexof<event, "depth"_>()
	};

	auto &column
	{
		event_column.at(idx)
	};

	uint64_t depth;
	column(byte_view<string_view>(event_idx), [&depth]
	(const string_view &binary)
	{
		depth = byte_view<uint64_t>(binary);
	});

	return state_root(out, room_id, event_idx, depth);
}

ircd::string_view
ircd::m::dbs::state_root(const mutable_buffer &out,
                         const id::room &room_id,
                         const id::event &event_id,
                         const uint64_t &depth)
{
	return state_root(out, room_id, index(event_id), depth);
}

ircd::string_view
ircd::m::dbs::state_root(const mutable_buffer &out,
                         const id::room &room_id,
                         const event::idx &event_idx,
                         const uint64_t &depth)
{
	char keybuf[ROOM_EVENTS_KEY_MAX_SIZE]; const auto key
	{
		room_events_key(keybuf, room_id, depth, event_idx)
	};

	string_view ret;
	room_events(key, [&out, &ret](const string_view &val)
	{
		ret = { data(out), copy(out, val) };
	});

	return ret;
}

//
// Database descriptors
//

//
// event_idx
//

decltype(ircd::m::dbs::desc::events__event_idx__block__size)
ircd::m::dbs::desc::events__event_idx__block__size
{
	{ "name",     "ircd.m.dbs.events._event_idx.block.size" },
	{ "default",  512L                                      },
};

decltype(ircd::m::dbs::desc::events__event_idx__meta_block__size)
ircd::m::dbs::desc::events__event_idx__meta_block__size
{
	{ "name",     "ircd.m.dbs.events._event_idx.meta_block.size" },
	{ "default",  4096L                                          },
};

decltype(ircd::m::dbs::desc::events__event_idx__cache__size)
ircd::m::dbs::desc::events__event_idx__cache__size
{
	{
		{ "name",     "ircd.m.dbs.events._event_idx.cache.size" },
		{ "default",  long(64_MiB)                              },
	}, []
	{
		const size_t &value{events__event_idx__cache__size};
		db::capacity(db::cache(event_idx), value);
	}
};

decltype(ircd::m::dbs::desc::events__event_idx__cache_comp__size)
ircd::m::dbs::desc::events__event_idx__cache_comp__size
{
	{
		{ "name",     "ircd.m.dbs.events._event_idx.cache_comp.size" },
		{ "default",  long(16_MiB)                                   },
	}, []
	{
		const size_t &value{events__event_idx__cache_comp__size};
		db::capacity(db::cache_compressed(event_idx), value);
	}
};

decltype(ircd::m::dbs::desc::events__event_idx__bloom__bits)
ircd::m::dbs::desc::events__event_idx__bloom__bits
{
	{ "name",     "ircd.m.dbs.events._event_idx.bloom.bits" },
	{ "default",  10L                                       },
};

const ircd::db::descriptor
ircd::m::dbs::desc::events__event_idx
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
	bool(events_cache_enable)? -1 : 0, //uses conf item

	// cache size for compressed assets
	bool(events_cache_comp_enable)? -1 : 0,

	// bloom filter bits
	size_t(events__event_idx__bloom__bits),

	// expect queries hit
	false,

	// block size
	size_t(events__event_idx__block__size),

	// meta_block size
	size_t(events__event_idx__meta_block__size),
};

//
// event_json
//

decltype(ircd::m::dbs::desc::events__event_json__block__size)
ircd::m::dbs::desc::events__event_json__block__size
{
	{ "name",     "ircd.m.dbs.events._event_json.block.size" },
	{ "default",  2048L                                      },
};

decltype(ircd::m::dbs::desc::events__event_json__meta_block__size)
ircd::m::dbs::desc::events__event_json__meta_block__size
{
	{ "name",     "ircd.m.dbs.events._event_json.meta_block.size" },
	{ "default",  512L                                            },
};

decltype(ircd::m::dbs::desc::events__event_json__cache__size)
ircd::m::dbs::desc::events__event_json__cache__size
{
	{
		{ "name",     "ircd.m.dbs.events._event_json.cache.size" },
		{ "default",  long(64_MiB)                               },
	}, []
	{
		const size_t &value{events__event_json__cache__size};
		db::capacity(db::cache(event_json), value);
	}
};

decltype(ircd::m::dbs::desc::events__event_json__cache_comp__size)
ircd::m::dbs::desc::events__event_json__cache_comp__size
{
	{
		{ "name",     "ircd.m.dbs.events._event_json.cache_comp.size" },
		{ "default",  long(0_MiB)                                     },
	}, []
	{
		const size_t &value{events__event_json__cache_comp__size};
		db::capacity(db::cache_compressed(event_json), value);
	}
};

decltype(ircd::m::dbs::desc::events__event_json__bloom__bits)
ircd::m::dbs::desc::events__event_json__bloom__bits
{
	{ "name",     "ircd.m.dbs.events._event_json.bloom.bits" },
	{ "default",  9L                                         },
};

const ircd::db::descriptor
ircd::m::dbs::desc::events__event_json
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
	bool(events_cache_enable)? -1 : 0, //uses conf item

	// cache size for compressed assets
	bool(events_cache_comp_enable)? -1 : 0,

	// bloom filter bits
	size_t(events__event_json__bloom__bits),

	// expect queries hit
	true,

	// block size
	size_t(events__event_json__block__size),

	// meta_block size
	size_t(events__event_json__meta_block__size),
};

//
// event_refs
//

decltype(ircd::m::dbs::desc::events__event_refs__block__size)
ircd::m::dbs::desc::events__event_refs__block__size
{
	{ "name",     "ircd.m.dbs.events._event_refs.block.size" },
	{ "default",  512L                                       },
};

decltype(ircd::m::dbs::desc::events__event_refs__meta_block__size)
ircd::m::dbs::desc::events__event_refs__meta_block__size
{
	{ "name",     "ircd.m.dbs.events._event_refs.meta_block.size" },
	{ "default",  512L                                            },
};

decltype(ircd::m::dbs::desc::events__event_refs__cache__size)
ircd::m::dbs::desc::events__event_refs__cache__size
{
	{
		{ "name",     "ircd.m.dbs.events._event_refs.cache.size" },
		{ "default",  long(16_MiB)                               },
	}, []
	{
		const size_t &value{events__event_refs__cache__size};
		db::capacity(db::cache(event_refs), value);
	}
};

decltype(ircd::m::dbs::desc::events__event_refs__cache_comp__size)
ircd::m::dbs::desc::events__event_refs__cache_comp__size
{
	{
		{ "name",     "ircd.m.dbs.events._event_refs.cache_comp.size" },
		{ "default",  long(0_MiB)                                     },
	}, []
	{
		const size_t &value{events__event_refs__cache_comp__size};
		db::capacity(db::cache_compressed(event_refs), value);
	}
};

ircd::string_view
ircd::m::dbs::reflect(const ref &type)
{
	switch(type)
	{
		case ref::NEXT:                  return "NEXT";
		case ref::NEXT_AUTH:             return "NEXT_AUTH";
		case ref::NEXT_STATE:            return "NEXT_STATE";
		case ref::PREV_STATE:            return "PREV_STATE";
		case ref::M_RECEIPT__M_READ:     return "M_RECEIPT__M_READ";
		case ref::M_RELATES__M_REPLY:    return "M_RELATES__M_REPLY";
		case ref::M_ROOM_REDACTION:      return "M_ROOM_REDACTION";
	}

	return "????";
}

ircd::string_view
ircd::m::dbs::event_refs_key(const mutable_buffer &out,
                             const event::idx &tgt,
                             const ref &type,
                             const event::idx &src)
{
	assert((src & ref_mask) == 0);
	assert(size(out) >= sizeof(event::idx) * 2);
	event::idx *const &key
	{
		reinterpret_cast<event::idx *>(data(out))
	};

	key[0] = tgt;
	key[1] = src;
	key[1] |= uint64_t(type) << ref_shift;
	return string_view
	{
		data(out), data(out) + sizeof(event::idx) * 2
	};
}

std::tuple<ircd::m::dbs::ref, ircd::m::event::idx>
ircd::m::dbs::event_refs_key(const string_view &amalgam)
{
	const event::idx key
	{
		byte_view<event::idx>{amalgam}
	};

	return
	{
		ref(key >> ref_shift), key & ~ref_mask
	};
}

const ircd::db::prefix_transform
ircd::m::dbs::desc::events__event_refs__pfx
{
	"_event_refs",
	[](const string_view &key)
	{
		return size(key) >= sizeof(event::idx) * 2;
	},

	[](const string_view &key)
	{
		assert(size(key) >= sizeof(event::idx));
		return string_view
		{
			data(key), data(key) + sizeof(event::idx)
		};
	}
};

const ircd::db::comparator
ircd::m::dbs::desc::events__event_refs__cmp
{
	"_event_refs",

	// less
	[](const string_view &a, const string_view &b)
	{
		static const size_t half(sizeof(event::idx));
		static const size_t full(half * 2);

		assert(size(a) >= half);
		assert(size(b) >= half);
		const event::idx *const key[2]
		{
			reinterpret_cast<const event::idx *>(data(a)),
			reinterpret_cast<const event::idx *>(data(b)),
		};

		return
			key[0][0] < key[1][0]?   true:
			key[0][0] > key[1][0]?   false:
			size(a) < size(b)?       true:
			size(a) > size(b)?       false:
			size(a) == half?         false:
			key[0][1] < key[1][1]?   true:
			                         false;
	},

	// equal
	[](const string_view &a, const string_view &b)
	{
		return size(a) == size(b) && memcmp(data(a), data(b), size(a)) == 0;
	}
};

const ircd::db::descriptor
ircd::m::dbs::desc::events__event_refs
{
	// name
	"_event_refs",

	// explanation
	R"(Inverse reference graph of events.

	event_idx | ref, event_idx => --

	The first part of the key is the event being referenced. The second part
	of the key is the event which refers to the first event somewhere in its
	prev_events references. The event_idx in the second part of the key also
	contains a dbs::ref type in its highest order byte so we can store
	different kinds of references.

	The prefix transform is in effect; an event may be referenced multiple
	times. We can find all the events we have which reference a target, and
	why. The database must already contain both events (hence they have
	event::idx numbers).

	The value is currently unused/empty; we may eventually store metadata with
	information about this reference (i.e. is depth adjacent? is the ref
	redundant with another in the same event and should not be made? etc).

	)",

	// typing (key, value)
	{
		typeid(uint64_t), typeid(string_view)
	},

	// options
	{},

	// comparator
	events__event_refs__cmp,

	// prefix transform
	events__event_refs__pfx,

	// drop column
	false,

	// cache size
	bool(events_cache_enable)? -1 : 0, //uses conf item

	// cache size for compressed assets
	bool(events_cache_comp_enable)? -1 : 0,

	// bloom filter bits
	0,

	// expect queries hit
	true,

	// block size
	size_t(events__event_refs__block__size),

	// meta_block size
	size_t(events__event_refs__meta_block__size),
};

//
// event_horizon
//

decltype(ircd::m::dbs::desc::events__event_horizon__block__size)
ircd::m::dbs::desc::events__event_horizon__block__size
{
	{ "name",     "ircd.m.dbs.events._event_horizon.block.size" },
	{ "default",  512L                                          },
};

decltype(ircd::m::dbs::desc::events__event_horizon__meta_block__size)
ircd::m::dbs::desc::events__event_horizon__meta_block__size
{
	{ "name",     "ircd.m.dbs.events._event_horizon.meta_block.size" },
	{ "default",  1024L                                              },
};

decltype(ircd::m::dbs::desc::events__event_horizon__cache__size)
ircd::m::dbs::desc::events__event_horizon__cache__size
{
	{
		{ "name",     "ircd.m.dbs.events._event_horizon.cache.size" },
		{ "default",  long(16_MiB)                                  },
	}, []
	{
		const size_t &value{events__event_horizon__cache__size};
		db::capacity(db::cache(event_horizon), value);
	}
};

decltype(ircd::m::dbs::desc::events__event_horizon__cache_comp__size)
ircd::m::dbs::desc::events__event_horizon__cache_comp__size
{
	{
		{ "name",     "ircd.m.dbs.events._event_horizon.cache_comp.size" },
		{ "default",  long(0_MiB)                                        },
	}, []
	{
		const size_t &value{events__event_horizon__cache_comp__size};
		db::capacity(db::cache_compressed(event_horizon), value);
	}
};

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
		consume(buf, copy(buf, "\0"_sv));
		consume(buf, copy(buf, byte_view<string_view>(event_idx)));
	}

	const string_view ret
	{
		data(out), data(buf)
	};

	assert(size(ret) == size(event_id) || size(ret) == size(event_id) + sizeof(event::idx) + 1);
	return ret;
}

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

const ircd::db::prefix_transform
ircd::m::dbs::desc::events__event_horizon__pfx
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
ircd::m::dbs::desc::events__event_horizon
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
	events__event_horizon__pfx,

	// drop column
	false,

	// cache size
	bool(events_cache_enable)? -1 : 0, //uses conf item

	// cache size for compressed assets
	bool(events_cache_comp_enable)? -1 : 0,

	// bloom filter bits
	0,

	// expect queries hit
	false,

	// block size
	size_t(events__event_horizon__block__size),

	// meta_block size
	size_t(events__event_horizon__meta_block__size),
};

//
// event_sender
//

decltype(ircd::m::dbs::desc::events__event_sender__block__size)
ircd::m::dbs::desc::events__event_sender__block__size
{
	{ "name",     "ircd.m.dbs.events._event_sender.block.size" },
	{ "default",  512L                                         },
};

decltype(ircd::m::dbs::desc::events__event_sender__meta_block__size)
ircd::m::dbs::desc::events__event_sender__meta_block__size
{
	{ "name",     "ircd.m.dbs.events._event_sender.meta_block.size" },
	{ "default",  4096L                                             },
};

decltype(ircd::m::dbs::desc::events__event_sender__cache__size)
ircd::m::dbs::desc::events__event_sender__cache__size
{
	{
		{ "name",     "ircd.m.dbs.events._event_sender.cache.size" },
		{ "default",  long(16_MiB)                                 },
	}, []
	{
		const size_t &value{events__event_sender__cache__size};
		db::capacity(db::cache(event_sender), value);
	}
};

decltype(ircd::m::dbs::desc::events__event_sender__cache_comp__size)
ircd::m::dbs::desc::events__event_sender__cache_comp__size
{
	{
		{ "name",     "ircd.m.dbs.events._event_sender.cache_comp.size" },
		{ "default",  long(0_MiB)                                       },
	}, []
	{
		const size_t &value{events__event_sender__cache_comp__size};
		db::capacity(db::cache_compressed(event_sender), value);
	}
};

ircd::string_view
ircd::m::dbs::event_sender_key(const mutable_buffer &out,
                               const user::id &user_id,
                               const event::idx &event_idx)
{
	return event_sender_key(out, user_id.host(), user_id.local(), event_idx);
}

ircd::string_view
ircd::m::dbs::event_sender_key(const mutable_buffer &out_,
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
		consume(out, copy(out, "\0"_sv));
		consume(out, copy(out, byte_view<string_view>(event_idx)));
	}

	return { data(out_), data(out) };
}

std::tuple<ircd::string_view, ircd::m::event::idx>
ircd::m::dbs::event_sender_key(const string_view &amalgam)
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

const ircd::db::prefix_transform
ircd::m::dbs::desc::events__event_sender__pfx
{
	"_event_sender",
	[](const string_view &key)
	{
		return has(key, '@');
	},

	[](const string_view &key)
	{
		const auto &parts
		{
			split(key, '@')
		};

		return parts.first;
	}
};

const ircd::db::descriptor
ircd::m::dbs::desc::events__event_sender
{
	// name
	"_event_sender",

	// explanation
	R"(Index of senders to their events.

	origin | localpart, event_idx => --

	The senders of events are indexes by this column. This allows for all
	events from a sender to be iterated. Additionally, all events from a
	server and all known servers can be iterated from this column.

	They key is made from a user mxid and an event_id, where the mxid is
	part-swapped so the origin comes first, and the @localpart comes after.
	Lookups can be performed for an origin or a full user_mxid.

	The prefix transform is in effect; the prefix domain is the origin. We
	can efficiently iterate all events from an origin. We can slightly less
	efficiently iterate all users from an origin, as well as iterate all
	origins known.

	Note that the indexer of this column ignores the actual "origin" field
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
	events__event_sender__pfx,

	// drop column
	false,

	// cache size
	bool(events_cache_enable)? -1 : 0, //uses conf item

	// cache size for compressed assets
	bool(events_cache_comp_enable)? -1 : 0,

	// bloom filter bits
	0,

	// expect queries hit
	false,

	// block size
	size_t(events__event_sender__block__size),

	// meta_block size
	size_t(events__event_sender__meta_block__size),
};

//
// event_type
//

decltype(ircd::m::dbs::desc::events__event_type__block__size)
ircd::m::dbs::desc::events__event_type__block__size
{
	{ "name",     "ircd.m.dbs.events._event_type.block.size" },
	{ "default",  512L                                       },
};

decltype(ircd::m::dbs::desc::events__event_type__meta_block__size)
ircd::m::dbs::desc::events__event_type__meta_block__size
{
	{ "name",     "ircd.m.dbs.events._event_type.meta_block.size" },
	{ "default",  4096L                                           },
};

decltype(ircd::m::dbs::desc::events__event_type__cache__size)
ircd::m::dbs::desc::events__event_type__cache__size
{
	{
		{ "name",     "ircd.m.dbs.events._event_type.cache.size" },
		{ "default",  long(16_MiB)                               },
	}, []
	{
		const size_t &value{events__event_type__cache__size};
		db::capacity(db::cache(event_type), value);
	}
};

decltype(ircd::m::dbs::desc::events__event_type__cache_comp__size)
ircd::m::dbs::desc::events__event_type__cache_comp__size
{
	{
		{ "name",     "ircd.m.dbs.events._event_type.cache_comp.size" },
		{ "default",  long(0_MiB)                                     },
	}, []
	{
		const size_t &value{events__event_type__cache_comp__size};
		db::capacity(db::cache_compressed(event_type), value);
	}
};

ircd::string_view
ircd::m::dbs::event_type_key(const mutable_buffer &out_,
                             const string_view &type,
                             const event::idx &event_idx)
{
	assert(size(out_) >= EVENT_TYPE_KEY_MAX_SIZE);

	mutable_buffer out{out_};
	consume(out, copy(out, type));
	consume(out, copy(out, "\0"_sv));
	consume(out, copy(out, byte_view<string_view>(event_idx)));
	return { data(out_), data(out) };
}

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

const ircd::db::prefix_transform
ircd::m::dbs::desc::events__event_type__pfx
{
	"_event_type",
	[](const string_view &key)
	{
		return has(key, '\0');
	},

	[](const string_view &key)
	{
		const auto &parts
		{
			split(key, '\0')
		};

		return parts.first;
	}
};

const ircd::db::descriptor
ircd::m::dbs::desc::events__event_type
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
	events__event_type__pfx,

	// drop column
	false,

	// cache size
	bool(events_cache_enable)? -1 : 0, //uses conf item

	// cache size for compressed assets
	bool(events_cache_comp_enable)? -1 : 0,

	// bloom filter bits
	0,

	// expect queries hit
	false,

	// block size
	size_t(events__event_type__block__size),

	// meta_block size
	size_t(events__event_type__meta_block__size),
};

//
// room_head
//

decltype(ircd::m::dbs::desc::events__room_head__block__size)
ircd::m::dbs::desc::events__room_head__block__size
{
	{ "name",     "ircd.m.dbs.events._room_head.block.size" },
	{ "default",  4096L                                     },
};

decltype(ircd::m::dbs::desc::events__room_head__meta_block__size)
ircd::m::dbs::desc::events__room_head__meta_block__size
{
	{ "name",     "ircd.m.dbs.events._room_head.meta_block.size" },
	{ "default",  4096L                                          },
};

decltype(ircd::m::dbs::desc::events__room_head__cache__size)
ircd::m::dbs::desc::events__room_head__cache__size
{
	{
		{ "name",     "ircd.m.dbs.events._room_head.cache.size" },
		{ "default",  long(8_MiB)                               },
	}, []
	{
		const size_t &value{events__room_head__cache__size};
		db::capacity(db::cache(room_head), value);
	}
};

/// prefix transform for room_id,event_id in room_id
///
const ircd::db::prefix_transform
ircd::m::dbs::desc::events__room_head__pfx
{
	"_room_head",
	[](const string_view &key)
	{
		return has(key, "\0"_sv);
	},

	[](const string_view &key)
	{
		return split(key, "\0"_sv).first;
	}
};

ircd::string_view
ircd::m::dbs::room_head_key(const mutable_buffer &out_,
                            const id::room &room_id,
                            const id::event &event_id)
{
	mutable_buffer out{out_};
	consume(out, copy(out, room_id));
	consume(out, copy(out, "\0"_sv));
	consume(out, copy(out, event_id));
	return { data(out_), data(out) };
}

ircd::string_view
ircd::m::dbs::room_head_key(const string_view &amalgam)
{
	const auto &key
	{
		lstrip(amalgam, "\0"_sv)
	};

	return
	{
		key
	};
}

/// This column stores unreferenced (head) events for a room.
///
const ircd::db::descriptor
ircd::m::dbs::desc::events__room_head
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
	events__room_head__pfx,

	// drop column
	false,

	// cache size
	bool(events_cache_enable)? -1 : 0,

	// cache size for compressed assets
	0, //no compresed cache

	// bloom filter bits
	0, //table too ephemeral for bloom generation/usefulness

	// expect queries hit
	false,

	// block size
	size_t(events__room_head__block__size),

	// meta_block size
	size_t(events__room_head__meta_block__size),

	// compression
	{}, // no compression for this column
};

//
// room_events
//

decltype(ircd::m::dbs::desc::events__room_events__block__size)
ircd::m::dbs::desc::events__room_events__block__size
{
	{ "name",     "ircd.m.dbs.events._room_events.block.size" },
	{ "default",  512L                                        },
};

decltype(ircd::m::dbs::desc::events__room_events__meta_block__size)
ircd::m::dbs::desc::events__room_events__meta_block__size
{
	{ "name",     "ircd.m.dbs.events._room_events.meta_block.size" },
	{ "default",  16384L                                           },
};

decltype(ircd::m::dbs::desc::events__room_events__cache__size)
ircd::m::dbs::desc::events__room_events__cache__size
{
	{
		{ "name",     "ircd.m.dbs.events._room_events.cache.size" },
		{ "default",  long(32_MiB)                                },
	}, []
	{
		const size_t &value{events__room_events__cache__size};
		db::capacity(db::cache(room_events), value);
	}
};

decltype(ircd::m::dbs::desc::events__room_events__cache_comp__size)
ircd::m::dbs::desc::events__room_events__cache_comp__size
{
	{
		{ "name",     "ircd.m.dbs.events._room_events.cache_comp.size" },
		{ "default",  long(16_MiB)                                     },
	}, []
	{
		const size_t &value{events__room_events__cache_comp__size};
		db::capacity(db::cache_compressed(room_events), value);
	}
};

/// Prefix transform for the events__room_events. The prefix here is a room_id
/// and the suffix is the depth+event_id concatenation.
/// for efficient sequences
///
const ircd::db::prefix_transform
ircd::m::dbs::desc::events__room_events__pfx
{
	"_room_events",

	[](const string_view &key)
	{
		return has(key, "\0"_sv);
	},

	[](const string_view &key)
	{
		return split(key, "\0"_sv).first;
	}
};

/// Comparator for the events__room_events. The goal here is to sort the
/// events within a room by their depth from highest to lowest, so the
/// highest depth is hit first when a room is sought from this column.
///
const ircd::db::comparator
ircd::m::dbs::desc::events__room_events__cmp
{
	"_room_events",

	// less
	[](const string_view &a, const string_view &b)
	{
		static const auto &pt
		{
			events__room_events__pfx
		};

		// Extract the prefix from the keys
		const string_view pre[2]
		{
			pt.get(a),
			pt.get(b),
		};

		if(size(pre[0]) != size(pre[1]))
			return size(pre[0]) < size(pre[1]);

		if(pre[0] != pre[1])
			return pre[0] < pre[1];

		// After the prefix is the depth + event_idx
		const string_view post[2]
		{
			a.substr(size(pre[0])),
			b.substr(size(pre[1])),
		};

		// These conditions are matched on some queries when the user only
		// supplies a room id.

		if(empty(post[0]))
			return true;

		if(empty(post[1]))
			return false;

		// Distill out the depth and event_idx integers
		const std::pair<uint64_t, event::idx> pair[2]
		{
			room_events_key(post[0]),
			room_events_key(post[1])
		};

		// When two events are at the same depth sort by index (the sequence
		// number given as they were admitted into the system) otherwise
		// sort by depth. Note this is a reverse order comparison.
		return std::get<0>(pair[1]) != std::get<0>(pair[0])?
			std::get<0>(pair[1]) < std::get<0>(pair[0]):
			std::get<1>(pair[1]) < std::get<1>(pair[0]);
	},

	// equal
	[](const string_view &a, const string_view &b)
	{
		return a == b;
	}
};

ircd::string_view
ircd::m::dbs::room_events_key(const mutable_buffer &out_,
                              const id::room &room_id,
                              const uint64_t &depth)
{
	const const_buffer depth_cb
	{
		reinterpret_cast<const char *>(&depth), sizeof(depth)
	};

	mutable_buffer out{out_};
	consume(out, copy(out, room_id));
	consume(out, copy(out, "\0"_sv));
	consume(out, copy(out, depth_cb));
	return { data(out_), data(out) };
}

ircd::string_view
ircd::m::dbs::room_events_key(const mutable_buffer &out_,
                              const id::room &room_id,
                              const uint64_t &depth,
                              const event::idx &event_idx)
{
	const const_buffer depth_cb
	{
		reinterpret_cast<const char *>(&depth), sizeof(depth)
	};

	const const_buffer event_idx_cb
	{
		reinterpret_cast<const char *>(&event_idx), sizeof(event_idx)
	};

	mutable_buffer out{out_};
	consume(out, copy(out, room_id));
	consume(out, copy(out, "\0"_sv));
	consume(out, copy(out, depth_cb));
	consume(out, copy(out, event_idx_cb));
	return { data(out_), data(out) };
}

std::pair<uint64_t, ircd::m::event::idx>
ircd::m::dbs::room_events_key(const string_view &amalgam)
{
	assert(size(amalgam) >= 1 + 8 + 8 || size(amalgam) == 1 + 8);
	assert(amalgam.front() == '\0');

	const uint64_t &depth
	{
		*reinterpret_cast<const uint64_t *>(data(amalgam) + 1)
	};

	const event::idx &event_idx
	{
		size(amalgam) >= 1 + 8 + 8?
			*reinterpret_cast<const uint64_t *>(data(amalgam) + 1 + 8):
			std::numeric_limits<uint64_t>::max()
	};

	// Make sure these are copied rather than ever returning references in
	// a tuple because the chance the integers will be aligned is low.
	return { depth, event_idx };
}

/// This column stores events in sequence in a room. Consider the following:
///
/// [room_id | depth + event_idx => state_root]
///
/// The key is composed from three parts:
///
/// - `room_id` is the official prefix, bounding the sequence. That means we
/// make a blind query with just a room_id and get to the beginning of the
/// sequence, then iterate until we stop before the next room_id (upper bound).
///
/// - `depth` is the ordering. Within the sequence, all elements are ordered by
/// depth from HIGHEST TO LOWEST. The sequence will start at the highest depth.
/// NOTE: Depth is a fixed 8 byte binary integer.
///
/// - `event_idx` is the key suffix. This column serves to sequence all events
/// within a room ordered by depth. There may be duplicate room_id|depth
/// prefixing but the event_idx suffix gives the key total uniqueness.
/// NOTE: event_idx is a fixed 8 byte binary integer.
///
/// The value is then used to store the node ID of the state tree root at this
/// event. Nodes of the state tree are stored in the state_node column. From
/// that root node the state of the room at the time of this event_id can be
/// queried.
///
/// There is one caveat here: we can't directly take a room_id and an event_idx
/// and make a trivial query to find the state root, since the depth number
/// gets in the way. Rather than creating yet another column without the depth,
/// for the time being, we pay the cost of an extra query to events_depth and
/// find that missing piece to make the exact query with all three key parts.
///
const ircd::db::descriptor
ircd::m::dbs::desc::events__room_events
{
	// name
	"_room_events",

	// explanation
	R"(Indexes events in timeline sequence for a room; maps to m::state root.

	[room_id | depth + event_idx => state_root]

	)",

	// typing (key, value)
	{
		typeid(string_view), typeid(string_view)
	},

	// options
	{},

	// comparator
	events__room_events__cmp,

	// prefix transform
	events__room_events__pfx,

	// drop column
	false,

	// cache size
	bool(events_cache_enable)? -1 : 0,

	// cache size for compressed assets
	bool(events_cache_comp_enable)? -1 : 0,

	// bloom filter bits
	0, // no bloom filter because of possible comparator issues

	// expect queries hit
	true,

	// block size
	size_t(events__room_events__block__size),

	// meta_block size
	size_t(events__room_events__meta_block__size),
};

//
// joined sequential
//

decltype(ircd::m::dbs::desc::events__room_joined__block__size)
ircd::m::dbs::desc::events__room_joined__block__size
{
	{ "name",     "ircd.m.dbs.events._room_joined.block.size" },
	{ "default",  512L                                        },
};

decltype(ircd::m::dbs::desc::events__room_joined__meta_block__size)
ircd::m::dbs::desc::events__room_joined__meta_block__size
{
	{ "name",     "ircd.m.dbs.events._room_joined.meta_block.size" },
	{ "default",  8192L                                            },
};

decltype(ircd::m::dbs::desc::events__room_joined__cache__size)
ircd::m::dbs::desc::events__room_joined__cache__size
{
	{
		{ "name",     "ircd.m.dbs.events._room_joined.cache.size" },
		{ "default",  long(8_MiB)                                 },
	}, []
	{
		const size_t &value{events__room_joined__cache__size};
		db::capacity(db::cache(room_joined), value);
	}
};

decltype(ircd::m::dbs::desc::events__room_joined__cache_comp__size)
ircd::m::dbs::desc::events__room_joined__cache_comp__size
{
	{
		{ "name",     "ircd.m.dbs.events._room_joined.cache_comp.size" },
		{ "default",  long(8_MiB)                                      },
	}, []
	{
		const size_t &value{events__room_joined__cache_comp__size};
		db::capacity(db::cache_compressed(room_joined), value);
	}
};

decltype(ircd::m::dbs::desc::events__room_joined__bloom__bits)
ircd::m::dbs::desc::events__room_joined__bloom__bits
{
	{ "name",     "ircd.m.dbs.events._room_joined.bloom.bits" },
	{ "default",  6L                                          },
};

/// Prefix transform for the events__room_joined
///
const ircd::db::prefix_transform
ircd::m::dbs::desc::events__room_joined__pfx
{
	"_room_joined",

	[](const string_view &key)
	{
		return has(key, "\0"_sv);
	},

	[](const string_view &key)
	{
		return split(key, "\0"_sv).first;
	}
};

ircd::string_view
ircd::m::dbs::room_joined_key(const mutable_buffer &out_,
                              const id::room &room_id,
                              const string_view &origin)
{
	mutable_buffer out{out_};
	consume(out, copy(out, room_id));
	consume(out, copy(out, "\0"_sv));
	consume(out, copy(out, origin));
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
	consume(out, copy(out, "\0"_sv));
	consume(out, copy(out, origin));
	consume(out, copy(out, member));
	return { data(out_), data(out) };
}

std::pair<ircd::string_view, ircd::string_view>
ircd::m::dbs::room_joined_key(const string_view &amalgam)
{
	const auto &key
	{
		lstrip(amalgam, "\0"_sv)
	};

	const auto &s
	{
		split(key, "@"_sv)
	};

	return
	{
		{ s.first },
		!empty(s.second)?
			string_view{begin(s.second) - 1, end(s.second)}:
			string_view{}
	};
}

const ircd::db::descriptor
ircd::m::dbs::desc::events__room_joined
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
	events__room_joined__pfx,

	// drop column
	false,

	// cache size
	bool(events_cache_enable)? -1 : 0,

	// cache size for compressed assets
	bool(events_cache_comp_enable)? -1 : 0,

	// bloom filter bits
	size_t(events__room_joined__bloom__bits),

	// expect queries hit
	false,

	// block size
	size_t(events__room_joined__block__size),

	// meta_block size
	size_t(events__room_joined__meta_block__size),
};

//
// room present state sequential
//

decltype(ircd::m::dbs::desc::events__room_state__block__size)
ircd::m::dbs::desc::events__room_state__block__size
{
	{ "name",     "ircd.m.dbs.events._room_state.block.size" },
	{ "default",  512L                                       },
};

decltype(ircd::m::dbs::desc::events__room_state__meta_block__size)
ircd::m::dbs::desc::events__room_state__meta_block__size
{
	{ "name",     "ircd.m.dbs.events._room_state.meta_block.size" },
	{ "default",  8192L                                           },
};

decltype(ircd::m::dbs::desc::events__room_state__cache__size)
ircd::m::dbs::desc::events__room_state__cache__size
{
	{
		{ "name",     "ircd.m.dbs.events._room_state.cache.size"  },
		{ "default",  long(16_MiB)                                },
	}, []
	{
		const size_t &value{events__room_state__cache__size};
		db::capacity(db::cache(room_state), value);
	}
};

decltype(ircd::m::dbs::desc::events__room_state__cache_comp__size)
ircd::m::dbs::desc::events__room_state__cache_comp__size
{
	{
		{ "name",     "ircd.m.dbs.events._room_state.cache_comp.size"  },
		{ "default",  long(8_MiB)                                      },
	}, []
	{
		const size_t &value{events__room_state__cache_comp__size};
		db::capacity(db::cache_compressed(room_state), value);
	}
};

decltype(ircd::m::dbs::desc::events__room_state__bloom__bits)
ircd::m::dbs::desc::events__room_state__bloom__bits
{
	{ "name",     "ircd.m.dbs.events._room_state.bloom.bits" },
	{ "default",  10L                                        },
};

/// prefix transform for type,state_key in room_id
///
/// This transform is special for concatenating room_id with type and state_key
/// in that order with prefix being the room_id (this may change to room_id+
/// type
///
const ircd::db::prefix_transform
ircd::m::dbs::desc::events__room_state__pfx
{
	"_room_state",
	[](const string_view &key)
	{
		return has(key, "\0"_sv);
	},

	[](const string_view &key)
	{
		return split(key, "\0"_sv).first;
	}
};

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

	if(likely(defined(type)))
	{
		consume(out, copy(out, "\0"_sv));
		consume(out, copy(out, type));
	}

	if(defined(state_key))
	{
		consume(out, copy(out, "\0"_sv));
		consume(out, copy(out, state_key));
	}

	return { data(out_), data(out) };
}

std::pair<ircd::string_view, ircd::string_view>
ircd::m::dbs::room_state_key(const string_view &amalgam)
{
	const auto &key
	{
		lstrip(amalgam, "\0"_sv)
	};

	const auto &s
	{
		split(key, "\0"_sv)
	};

	return
	{
		s.first, s.second
	};
}

const ircd::db::descriptor
ircd::m::dbs::desc::events__room_state
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
	events__room_state__pfx,

	// drop column
	false,

	// cache size
	bool(events_cache_enable)? -1 : 0,

	// cache size for compressed assets
	bool(events_cache_comp_enable)? -1 : 0,

	// bloom filter bits
	size_t(events__room_state__bloom__bits),

	// expect queries hit
	false,

	// block size
	size_t(events__room_state__block__size),

	// meta_block size
	size_t(events__room_state__meta_block__size),
};

//
// room all states sequential
//

decltype(ircd::m::dbs::desc::events__room_state_space__block__size)
ircd::m::dbs::desc::events__room_state_space__block__size
{
	{ "name",     "ircd.m.dbs.events._room_state_space.block.size" },
	{ "default",  512L                                             },
};

decltype(ircd::m::dbs::desc::events__room_state_space__meta_block__size)
ircd::m::dbs::desc::events__room_state_space__meta_block__size
{
	{ "name",     "ircd.m.dbs.events._room_state_space.meta_block.size" },
	{ "default",  8192L                                                 },
};

decltype(ircd::m::dbs::desc::events__room_state_space__cache__size)
ircd::m::dbs::desc::events__room_state_space__cache__size
{
	{
		{ "name",     "ircd.m.dbs.events._room_state_space.cache.size"  },
		{ "default",  long(16_MiB)                                      },
	}, []
	{
		const size_t &value{events__room_state_space__cache__size};
		db::capacity(db::cache(room_state_space), value);
	}
};

decltype(ircd::m::dbs::desc::events__room_state_space__cache_comp__size)
ircd::m::dbs::desc::events__room_state_space__cache_comp__size
{
	{
		{ "name",     "ircd.m.dbs.events._room_state_space.cache_comp.size"  },
		{ "default",  long(8_MiB)                                            },
	}, []
	{
		const size_t &value{events__room_state_space__cache_comp__size};
		db::capacity(db::cache_compressed(room_state_space), value);
	}
};

decltype(ircd::m::dbs::desc::events__room_state_space__bloom__bits)
ircd::m::dbs::desc::events__room_state_space__bloom__bits
{
	{ "name",     "ircd.m.dbs.events._room_state_space.bloom.bits" },
	{ "default",  10L                                              },
};

ircd::string_view
ircd::m::dbs::room_state_space_key(const mutable_buffer &out_,
                                   const id::room &room_id,
                                   const string_view &type)
{
	return room_state_space_key(out_, room_id, type, string_view{});
}

ircd::string_view
ircd::m::dbs::room_state_space_key(const mutable_buffer &out_,
                                   const id::room &room_id,
                                   const string_view &type,
                                   const string_view &state_key)
{
	return room_state_space_key(out_, room_id, type, state_key, -1L);
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

	if(!defined(type))
		return { data(out_), data(out) };

	consume(out, copy(out, "\0"_sv));
	consume(out, copy(out, type));

	if(!defined(state_key))
	{
		assert(depth < 0L && !event_idx);
		return { data(out_), data(out) };
	}

	consume(out, copy(out, "\0"_sv));
	consume(out, copy(out, state_key));
	consume(out, copy(out, "\0"_sv));

	if(depth < 0)
	{
		assert(!event_idx);
		return { data(out_), data(out) };
	}

	consume(out, copy(out, byte_view<string_view>(depth)));

	if(!event_idx)
		return { data(out_), data(out) };

	consume(out, copy(out, byte_view<string_view>(event_idx)));
	return { data(out_), data(out) };
}

ircd::m::dbs::room_state_space_key_parts
ircd::m::dbs::room_state_space_key(const string_view &amalgam)
{
	const auto &key
	{
		lstrip(amalgam, "\0"_sv)
	};

	const auto &type_split
	{
		split(key, "\0"_sv)
	};

	const auto &state_key_split
	{
		split(type_split.second, "\0"_sv)
	};

	const auto &type
	{
		type_split.first
	};

	const auto &state_key
	{
		state_key_split.first
	};

	const int64_t &depth
	{
		size(state_key_split.second) >= 8?
			int64_t(byte_view<int64_t>(state_key_split.second.substr(0, 8))):
			0L
	};

	const event::idx &event_idx
	{
		size(state_key_split.second) >= 16?
			event::idx(byte_view<event::idx>(state_key_split.second.substr(8, 8))):
			0L
	};

	return
	{
		type, state_key, depth, event_idx
	};
}

const ircd::db::prefix_transform
ircd::m::dbs::desc::events__room_state_space__pfx
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

const ircd::db::comparator
ircd::m::dbs::desc::events__room_state_space__cmp
{
	"_room_state_space",

	// less
	[](const string_view &a, const string_view &b)
	{
		static const auto &pt
		{
			events__room_state_space__pfx
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

		// Perform standard comparison over the typed tuple.
		const auto _a(room_state_space_key(post[0]));
		const auto _b(room_state_space_key(post[1]));
		return _a < _b;
	},

	// equal
	[](const string_view &a, const string_view &b)
	{
		return a == b;
	}
};

const ircd::db::descriptor
ircd::m::dbs::desc::events__room_state_space
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
	{},

	// prefix transform
	events__room_state_space__pfx,

	// drop column
	false,

	// cache size
	bool(events_cache_enable)? -1 : 0,

	// cache size for compressed assets
	bool(events_cache_comp_enable)? -1 : 0,

	// bloom filter bits
	size_t(events__room_state_space__bloom__bits),

	// expect queries hit
	false,

	// block size
	size_t(events__room_state_space__block__size),

	// meta_block size
	size_t(events__room_state_space__meta_block__size),
};

//
// state node
//

decltype(ircd::m::dbs::desc::events__state_node__block__size)
ircd::m::dbs::desc::events__state_node__block__size
{
	{ "name",     "ircd.m.dbs.events._state_node.block.size" },
	{ "default",  1024L                                      },
};

decltype(ircd::m::dbs::desc::events__state_node__meta_block__size)
ircd::m::dbs::desc::events__state_node__meta_block__size
{
	{ "name",     "ircd.m.dbs.events._state_node.meta_block.size" },
	{ "default",  1024L                                           },
};

decltype(ircd::m::dbs::desc::events__state_node__cache__size)
ircd::m::dbs::desc::events__state_node__cache__size
{
	{
		{ "name",     "ircd.m.dbs.events._state_node.cache.size"  },
		{ "default",  long(64_MiB)                                },
	}, []
	{
		const size_t &value{events__state_node__cache__size};
		db::capacity(db::cache(state_node), value);
	}
};

decltype(ircd::m::dbs::desc::events__state_node__cache_comp__size)
ircd::m::dbs::desc::events__state_node__cache_comp__size
{
	{
		{ "name",     "ircd.m.dbs.events._state_node.cache_comp.size"  },
		{ "default",  long(32_MiB)                                     },
	}, []
	{
		const size_t &value{events__state_node__cache_comp__size};
		db::capacity(db::cache_compressed(state_node), value);
	}
};

decltype(ircd::m::dbs::desc::events__state_node__bloom__bits)
ircd::m::dbs::desc::events__state_node__bloom__bits
{
	{ "name",     "ircd.m.dbs.events._state_node.bloom.bits" },
	{ "default",  0L                                         },
};

/// State nodes are pieces of the m::state:: b-tree. The key is the hash
/// of the value, which serves as the ID of the node when referenced in
/// the tree. see: m/state.h for details.
///
const ircd::db::descriptor
ircd::m::dbs::desc::events__state_node
{
	// name
	"_state_node",

	// explanation
	R"(Node data in the m::state b-tree.

	The key is the node_id (a hash of the node's value). The value is JSON.
	See the m::state system for more information.

	)",

	// typing (key, value)
	{
		typeid(ircd::string_view), typeid(ircd::string_view)
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
	bool(events_cache_enable)? -1 : 0,

	// cache size for compressed assets
	bool(events_cache_comp_enable)? -1 : 0,

	// bloom filter bits
	size_t(events__state_node__bloom__bits),

	// expect queries hit
	true,

	// block size
	size_t(events__state_node__block__size),

	// meta_block size
	size_t(events__state_node__meta_block__size),
};

//
// Direct column descriptors
//

decltype(ircd::m::dbs::desc::events___event__bloom__bits)
ircd::m::dbs::desc::events___event__bloom__bits
{
	{ "name",     "ircd.m.dbs.events.__event.bloom.bits" },
	{ "default",  8L                                     },
};

//
// event_id
//

decltype(ircd::m::dbs::desc::events__event_id__block__size)
ircd::m::dbs::desc::events__event_id__block__size
{
	{ "name",     "ircd.m.dbs.events.event_id.block.size"  },
	{ "default",  512L                                     },
};

decltype(ircd::m::dbs::desc::events__event_id__meta_block__size)
ircd::m::dbs::desc::events__event_id__meta_block__size
{
	{ "name",     "ircd.m.dbs.events.event_id.meta_block.size"  },
	{ "default",  512L                                          },
};

decltype(ircd::m::dbs::desc::events__event_id__cache__size)
ircd::m::dbs::desc::events__event_id__cache__size
{
	{
		{ "name",     "ircd.m.dbs.events.event_id.cache.size"  },
		{ "default",  long(32_MiB)                             },
	}, []
	{
		auto &column(event_column.at(json::indexof<event, "event_id"_>()));
		const size_t &value{events__event_id__cache__size};
		db::capacity(db::cache(column), value);
	}
};

decltype(ircd::m::dbs::desc::events__event_id__cache_comp__size)
ircd::m::dbs::desc::events__event_id__cache_comp__size
{
	{
		{ "name",     "ircd.m.dbs.events.event_id.cache_comp.size"  },
		{ "default",  long(16_MiB)                                  },
	}, []
	{
		auto &column(event_column.at(json::indexof<event, "event_id"_>()));
		const size_t &value{events__event_id__cache_comp__size};
		db::capacity(db::cache_compressed(column), value);
	}
};

const ircd::db::descriptor
ircd::m::dbs::desc::events_event_id
{
	// name
	"event_id",

	// explanation
	R"(Stores the event_id property of an event.

	As with all direct event columns the key is an event_idx and the value
	is the data for the event. It should be mentioned for this column
	specifically that event_id's are already saved in the _event_idx column
	however that is a mapping of event_id to event_idx whereas this is a
	mapping of event_idx to event_id.

	10.4
	MUST NOT exceed 255 bytes.

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
	bool(events_cache_enable)? -1 : 0,

	// cache size for compressed assets
	bool(events_cache_comp_enable)? -1 : 0,

	// bloom filter bits
	size_t(events___event__bloom__bits),

	// expect queries hit
	true,

	// block size
	size_t(events__event_id__block__size),

	// meta_block size
	size_t(events__event_id__meta_block__size),
};

//
// type
//

decltype(ircd::m::dbs::desc::events__type__block__size)
ircd::m::dbs::desc::events__type__block__size
{
	{ "name",     "ircd.m.dbs.events.type.block.size"  },
	{ "default",  512L                                 },
};

decltype(ircd::m::dbs::desc::events__type__meta_block__size)
ircd::m::dbs::desc::events__type__meta_block__size
{
	{ "name",     "ircd.m.dbs.events.type.meta_block.size"  },
	{ "default",  512L                                      },
};

decltype(ircd::m::dbs::desc::events__type__cache__size)
ircd::m::dbs::desc::events__type__cache__size
{
	{
		{ "name",     "ircd.m.dbs.events.type.cache.size"  },
		{ "default",  long(32_MiB)                         },
	}, []
	{
		auto &column(event_column.at(json::indexof<event, "type"_>()));
		const size_t &value{events__type__cache__size};
		db::capacity(db::cache(column), value);
	}
};

decltype(ircd::m::dbs::desc::events__type__cache_comp__size)
ircd::m::dbs::desc::events__type__cache_comp__size
{
	{
		{ "name",     "ircd.m.dbs.events.type.cache_comp.size"  },
		{ "default",  long(16_MiB)                              },
	}, []
	{
		auto &column(event_column.at(json::indexof<event, "type"_>()));
		const size_t &value{events__type__cache_comp__size};
		db::capacity(db::cache_compressed(column), value);
	}
};

const ircd::db::descriptor
ircd::m::dbs::desc::events_type
{
	// name
	"type",

	// explanation
	R"(Stores the type property of an event.

	10.1
	The type of event. This SHOULD be namespaced similar to Java package naming conventions
	e.g. 'com.example.subdomain.event.type'.

	10.4
	MUST NOT exceed 255 bytes.

	### developer note:
	key is event_idx number.
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
	bool(events_cache_enable)? -1 : 0,

	// cache size for compressed assets
	bool(events_cache_comp_enable)? -1 : 0,

	// bloom filter bits
	size_t(events___event__bloom__bits),

	// expect queries hit
	true,

	// block size
	size_t(events__type__block__size),

	// meta_block size
	size_t(events__type__meta_block__size),
};

//
// content
//

decltype(ircd::m::dbs::desc::events__content__block__size)
ircd::m::dbs::desc::events__content__block__size
{
	{ "name",     "ircd.m.dbs.events.content.block.size"  },
	{ "default",  2048L                                   },
};

decltype(ircd::m::dbs::desc::events__content__meta_block__size)
ircd::m::dbs::desc::events__content__meta_block__size
{
	{ "name",     "ircd.m.dbs.events.content.meta_block.size"  },
	{ "default",  512L                                         },
};

decltype(ircd::m::dbs::desc::events__content__cache__size)
ircd::m::dbs::desc::events__content__cache__size
{
	{
		{ "name",     "ircd.m.dbs.events.content.cache.size"  },
		{ "default",  long(48_MiB)                            },
	}, []
	{
		auto &column(event_column.at(json::indexof<event, "content"_>()));
		const size_t &value{events__content__cache__size};
		db::capacity(db::cache(column), value);
	}
};

decltype(ircd::m::dbs::desc::events__content__cache_comp__size)
ircd::m::dbs::desc::events__content__cache_comp__size
{
	{
		{ "name",     "ircd.m.dbs.events.content.cache_comp.size"  },
		{ "default",  long(16_MiB)                                 },
	}, []
	{
		auto &column(event_column.at(json::indexof<event, "content"_>()));
		const size_t &value{events__content__cache_comp__size};
		db::capacity(db::cache_compressed(column), value);
	}
};

const ircd::db::descriptor
ircd::m::dbs::desc::events_content
{
	// name
	"content",

	// explanation
	R"(Stores the content property of an event.

	10.1
	The fields in this object will vary depending on the type of event. When interacting
	with the REST API, this is the HTTP body.

	### developer note:
	Since events must not exceed 64 KiB the maximum size for the content is the remaining
	space after all the other fields for the event are rendered.

	key is event_idx number.
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
	bool(events_cache_enable)? -1 : 0,

	// cache size for compressed assets
	bool(events_cache_comp_enable)? -1 : 0,

	// bloom filter bits
	size_t(events___event__bloom__bits),

	// expect queries hit
	true,

	// block size
	size_t(events__content__block__size),

	// meta_block size
	size_t(events__content__meta_block__size),
};

//
// room_id
//

decltype(ircd::m::dbs::desc::events__room_id__block__size)
ircd::m::dbs::desc::events__room_id__block__size
{
	{ "name",     "ircd.m.dbs.events.room_id.block.size"  },
	{ "default",  512L                                    },
};

decltype(ircd::m::dbs::desc::events__room_id__meta_block__size)
ircd::m::dbs::desc::events__room_id__meta_block__size
{
	{ "name",     "ircd.m.dbs.events.room_id.meta_block.size"  },
	{ "default",  512L                                         },
};

decltype(ircd::m::dbs::desc::events__room_id__cache__size)
ircd::m::dbs::desc::events__room_id__cache__size
{
	{
		{ "name",     "ircd.m.dbs.events.room_id.cache.size"  },
		{ "default",  long(32_MiB)                            },
	}, []
	{
		auto &column(event_column.at(json::indexof<event, "room_id"_>()));
		const size_t &value{events__room_id__cache__size};
		db::capacity(db::cache(column), value);
	}
};

decltype(ircd::m::dbs::desc::events__room_id__cache_comp__size)
ircd::m::dbs::desc::events__room_id__cache_comp__size
{
	{
		{ "name",     "ircd.m.dbs.events.room_id.cache_comp.size"  },
		{ "default",  long(16_MiB)                                 },
	}, []
	{
		auto &column(event_column.at(json::indexof<event, "room_id"_>()));
		const size_t &value{events__room_id__cache_comp__size};
		db::capacity(db::cache_compressed(column), value);
	}
};

const ircd::db::descriptor
ircd::m::dbs::desc::events_room_id
{
	// name
	"room_id",

	// explanation
	R"(Stores the room_id property of an event.

	10.2 (apropos room events)
	Required. The ID of the room associated with this event.

	10.4
	MUST NOT exceed 255 bytes.

	### developer note:
	key is event_idx number.
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
	bool(events_cache_enable)? -1 : 0,

	// cache size for compressed assets
	bool(events_cache_comp_enable)? -1 : 0,

	// bloom filter bits
	size_t(events___event__bloom__bits),

	// expect queries hit
	true,

	// block size
	size_t(events__room_id__block__size),

	// meta_block size
	size_t(events__room_id__meta_block__size),
};

//
// sender
//

decltype(ircd::m::dbs::desc::events__sender__block__size)
ircd::m::dbs::desc::events__sender__block__size
{
	{ "name",     "ircd.m.dbs.events.sender.block.size"  },
	{ "default",  512L                                   },
};

decltype(ircd::m::dbs::desc::events__sender__meta_block__size)
ircd::m::dbs::desc::events__sender__meta_block__size
{
	{ "name",     "ircd.m.dbs.events.sender.meta_block.size"  },
	{ "default",  512L                                        },
};

decltype(ircd::m::dbs::desc::events__sender__cache__size)
ircd::m::dbs::desc::events__sender__cache__size
{
	{
		{ "name",     "ircd.m.dbs.events.sender.cache.size"  },
		{ "default",  long(32_MiB)                           },
	}, []
	{
		auto &column(event_column.at(json::indexof<event, "sender"_>()));
		const size_t &value{events__sender__cache__size};
		db::capacity(db::cache(column), value);
	}
};

decltype(ircd::m::dbs::desc::events__sender__cache_comp__size)
ircd::m::dbs::desc::events__sender__cache_comp__size
{
	{
		{ "name",     "ircd.m.dbs.events.sender.cache_comp.size"  },
		{ "default",  long(16_MiB)                                },
	}, []
	{
		auto &column(event_column.at(json::indexof<event, "sender"_>()));
		const size_t &value{events__sender__cache_comp__size};
		db::capacity(db::cache_compressed(column), value);
	}
};

const ircd::db::descriptor
ircd::m::dbs::desc::events_sender
{
	// name
	"sender",

	// explanation
	R"(Stores the sender property of an event.

	10.2 (apropos room events)
	Required. Contains the fully-qualified ID of the user who sent this event.

	10.4
	MUST NOT exceed 255 bytes.

	### developer note:
	key is event_idx number.
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
	bool(events_cache_enable)? -1 : 0,

	// cache size for compressed assets
	bool(events_cache_comp_enable)? -1 : 0,

	// bloom filter bits
	size_t(events___event__bloom__bits),

	// expect queries hit
	true,

	// block size
	size_t(events__sender__block__size),

	// meta_block size
	size_t(events__sender__meta_block__size),
};

//
// state_key
//

decltype(ircd::m::dbs::desc::events__state_key__block__size)
ircd::m::dbs::desc::events__state_key__block__size
{
	{ "name",     "ircd.m.dbs.events.state_key.block.size"  },
	{ "default",  512L                                      },
};

decltype(ircd::m::dbs::desc::events__state_key__meta_block__size)
ircd::m::dbs::desc::events__state_key__meta_block__size
{
	{ "name",     "ircd.m.dbs.events.state_key.meta_block.size"  },
	{ "default",  512L                                           },
};

decltype(ircd::m::dbs::desc::events__state_key__cache__size)
ircd::m::dbs::desc::events__state_key__cache__size
{
	{
		{ "name",     "ircd.m.dbs.events.state_key.cache.size"  },
		{ "default",  long(32_MiB)                              },
	}, []
	{
		auto &column(event_column.at(json::indexof<event, "state_key"_>()));
		const size_t &value{events__state_key__cache__size};
		db::capacity(db::cache(column), value);
	}
};

decltype(ircd::m::dbs::desc::events__state_key__cache_comp__size)
ircd::m::dbs::desc::events__state_key__cache_comp__size
{
	{
		{ "name",     "ircd.m.dbs.events.state_key.cache_comp.size"  },
		{ "default",  long(16_MiB)                                   },
	}, []
	{
		auto &column(event_column.at(json::indexof<event, "state_key"_>()));
		const size_t &value{events__state_key__cache_comp__size};
		db::capacity(db::cache_compressed(column), value);
	}
};

const ircd::db::descriptor
ircd::m::dbs::desc::events_state_key
{
	// name
	"state_key",

	// explanation
	R"(Stores the state_key property of an event.

	10.3 (apropos room state events)
	A unique key which defines the overwriting semantics for this piece of room state.
	This value is often a zero-length string. The presence of this key makes this event a
	State Event. The key MUST NOT start with '_'.

	10.4
	MUST NOT exceed 255 bytes.

	### developer note:
	key is event_idx number.
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
	bool(events_cache_enable)? -1 : 0,

	// cache size for compressed assets
	bool(events_cache_comp_enable)? -1 : 0,

	// bloom filter bits
	size_t(events___event__bloom__bits),

	// expect queries hit
	true,

	// block size
	size_t(events__state_key__block__size),

	// meta_block size
	size_t(events__state_key__meta_block__size),
};

//
// origin_server_ts
//

decltype(ircd::m::dbs::desc::events__origin_server_ts__block__size)
ircd::m::dbs::desc::events__origin_server_ts__block__size
{
	{ "name",     "ircd.m.dbs.events.origin_server_ts.block.size"  },
	{ "default",  256L                                             },
};

decltype(ircd::m::dbs::desc::events__origin_server_ts__meta_block__size)
ircd::m::dbs::desc::events__origin_server_ts__meta_block__size
{
	{ "name",     "ircd.m.dbs.events.origin_server_ts.meta_block.size"  },
	{ "default",  512L                                                  },
};

decltype(ircd::m::dbs::desc::events__origin_server_ts__cache__size)
ircd::m::dbs::desc::events__origin_server_ts__cache__size
{
	{
		{ "name",     "ircd.m.dbs.events.origin_server_ts.cache.size"  },
		{ "default",  long(16_MiB)                                     },
	}, []
	{
		auto &column(event_column.at(json::indexof<event, "origin_server_ts"_>()));
		const size_t &value{events__origin_server_ts__cache__size};
		db::capacity(db::cache(column), value);
	}
};

decltype(ircd::m::dbs::desc::events__origin_server_ts__cache_comp__size)
ircd::m::dbs::desc::events__origin_server_ts__cache_comp__size
{
	{
		{ "name",     "ircd.m.dbs.events.origin_server_ts.cache_comp.size"  },
		{ "default",  long(16_MiB)                                          },
	}, []
	{
		auto &column(event_column.at(json::indexof<event, "origin_server_ts"_>()));
		const size_t &value{events__origin_server_ts__cache_comp__size};
		db::capacity(db::cache_compressed(column), value);
	}
};

const ircd::db::descriptor
ircd::m::dbs::desc::events_origin_server_ts
{
	// name
	"origin_server_ts",

	// explanation
	R"(Stores the origin_server_ts property of an event.

	FEDERATION 4.1
	Timestamp in milliseconds on origin homeserver when this PDU was created.

	### developer note:
	key is event_idx number.
	value is a machine integer (binary)

	TODO: consider unsigned rather than time_t because of millisecond precision

	)",

	// typing (key, value)
	{
		typeid(uint64_t), typeid(time_t)
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
	bool(events_cache_enable)? -1 : 0,

	// cache size for compressed assets
	bool(events_cache_comp_enable)? -1 : 0,

	// bloom filter bits
	size_t(events___event__bloom__bits),

	// expect queries hit
	true,

	// block size
	size_t(events__origin_server_ts__block__size),

	// meta_block size
	size_t(events__origin_server_ts__meta_block__size),
};

//
// depth
//

decltype(ircd::m::dbs::desc::events__depth__block__size)
ircd::m::dbs::desc::events__depth__block__size
{
	{ "name",     "ircd.m.dbs.events.depth.block.size"  },
	{ "default",  256L                                  },
};

decltype(ircd::m::dbs::desc::events__depth__meta_block__size)
ircd::m::dbs::desc::events__depth__meta_block__size
{
	{ "name",     "ircd.m.dbs.events.depth.meta_block.size"  },
	{ "default",  512L                                       },
};

decltype(ircd::m::dbs::desc::events__depth__cache__size)
ircd::m::dbs::desc::events__depth__cache__size
{
	{
		{ "name",     "ircd.m.dbs.events.depth.cache.size"  },
		{ "default",  long(16_MiB)                          },
	}, []
	{
		auto &column(event_column.at(json::indexof<event, "depth"_>()));
		const size_t &value{events__depth__cache__size};
		db::capacity(db::cache(column), value);
	}
};

decltype(ircd::m::dbs::desc::events__depth__cache_comp__size)
ircd::m::dbs::desc::events__depth__cache_comp__size
{
	{
		{ "name",     "ircd.m.dbs.events.depth.cache_comp.size"  },
		{ "default",  long(16_MiB)                               },
	}, []
	{
		auto &column(event_column.at(json::indexof<event, "depth"_>()));
		const size_t &value{events__depth__cache_comp__size};
		db::capacity(db::cache_compressed(column), value);
	}
};

const ircd::db::descriptor
ircd::m::dbs::desc::events_depth
{
	// name
	"depth",

	// explanation
	R"(Stores the depth property of an event.

	### developer note:
	key is event_idx number. value is long integer
	)",

	// typing (key, value)
	{
		typeid(uint64_t), typeid(int64_t)
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
	bool(events_cache_enable)? -1 : 0,

	// cache size for compressed assets
	bool(events_cache_comp_enable)? -1 : 0,

	// bloom filter bits
	size_t(events___event__bloom__bits),

	// expect queries hit
	true,

	// block size
	size_t(events__depth__block__size),

	// meta_block size
	size_t(events__depth__meta_block__size),
};

//
// Other column descriptions
//

namespace ircd::m::dbs::desc
{
	// Deprecated / dropped columns.
	//
	// These have to be retained for users that have yet to open their
	// database with a newly released schema which has dropped a column
	// from the schema. If the legacy descriptor is not provided here then
	// the database will not know how to open the descriptor in order to
	// conduct the drop.

	extern const ircd::db::descriptor events_auth_events;
	extern const ircd::db::descriptor events_hashes;
	extern const ircd::db::descriptor events_membership;
	extern const ircd::db::descriptor events_origin;
	extern const ircd::db::descriptor events_prev_events;
	extern const ircd::db::descriptor events_prev_state;
	extern const ircd::db::descriptor events_redacts;
	extern const ircd::db::descriptor events_signatures;
	extern const ircd::db::descriptor events__event_auth;
	extern const ircd::db::comparator events__event_auth__cmp;
	extern const ircd::db::prefix_transform events__event_auth__pfx;
	extern const ircd::db::descriptor events__event_bad;

	//
	// Required by RocksDB
	//

	extern const ircd::db::descriptor events__default;
};

const ircd::db::prefix_transform
ircd::m::dbs::desc::events__event_auth__pfx
{
	"_event_auth",
	[](const string_view &key)
	{
		return size(key) >= sizeof(event::idx) * 2;
	},

	[](const string_view &key)
	{
		assert(size(key) >= sizeof(event::idx));
		return string_view
		{
			data(key), data(key) + sizeof(event::idx)
		};
	}
};

const ircd::db::comparator
ircd::m::dbs::desc::events__event_auth__cmp
{
	"_event_auth",

	// less
	[](const string_view &a, const string_view &b)
	{
		static const size_t half(sizeof(event::idx));
		static const size_t full(half * 2);

		assert(size(a) >= half);
		assert(size(b) >= half);
		const event::idx *const key[2]
		{
			reinterpret_cast<const event::idx *>(data(a)),
			reinterpret_cast<const event::idx *>(data(b)),
		};

		return
			key[0][0] < key[1][0]?   true:
			key[0][0] > key[1][0]?   false:
			size(a) < size(b)?       true:
			size(a) > size(b)?       false:
			size(a) == half?         false:
			key[0][1] < key[1][1]?   true:
			                         false;
	},

	// equal
	[](const string_view &a, const string_view &b)
	{
		return size(a) == size(b) && memcmp(data(a), data(b), size(a)) == 0;
	}
};

const ircd::db::descriptor
ircd::m::dbs::desc::events__event_auth
{
	// name
	"_event_auth",

	// explanation
	R"(Inverse reference graph of events.

	event_idx | ref, event_idx => --

	The first part of the key is the event being referenced. The second part
	of the key is the event which refers to the first event somewhere in its
	prev_events references. The event_idx in the second part of the key also
	contains a dbs::ref type in its highest order byte so we can store
	different kinds of references.

	The prefix transform is in effect; an event may be referenced multiple
	times. We can find all the events we have which reference a target, and
	why. The database must already contain both events (hence they have
	event::idx numbers).

	The value is currently unused/empty; we may eventually store metadata with
	information about this reference (i.e. is depth adjacent? is the ref
	redundant with another in the same event and should not be made? etc).

	)",

	// typing (key, value)
	{
		typeid(uint64_t), typeid(string_view)
	},

	// options
	{},

	// comparator
	events__event_auth__cmp,

	// prefix transform
	events__event_auth__pfx,

	// drop column
	true,
};

const ircd::db::descriptor
ircd::m::dbs::desc::events__event_bad
{
	// name
	"_event_bad",

	// explanation
	R"(

	This column is deprecated and has been dropped from the schema. This
	descriptor will erase its presence in the database upon next open.

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
	true,
};

const ircd::db::descriptor
ircd::m::dbs::desc::events_auth_events
{
	// name
	"auth_events",

	// explanation
	R"(

	This column is deprecated and has been dropped from the schema. This
	descriptor will erase its presence in the database upon next open.

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
	true,
};

const ircd::db::descriptor
ircd::m::dbs::desc::events_hashes
{
	// name
	"hashes",

	// explanation
	R"(

	This column is deprecated and has been dropped from the schema. This
	descriptor will erase its presence in the database upon next open.

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
	true,
};

const ircd::db::descriptor
ircd::m::dbs::desc::events_membership
{
	// name
	"membership",

	// explanation
	R"(

	This column is deprecated and has been dropped from the schema. This
	descriptor will erase its presence in the database upon next open.

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
	true,
};

const ircd::db::descriptor
ircd::m::dbs::desc::events_origin
{
	// name
	"origin",

	// explanation
	R"(

	This column is deprecated and has been dropped from the schema. This
	descriptor will erase its presence in the database upon next open.

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
	true,
};

const ircd::db::descriptor
ircd::m::dbs::desc::events_prev_events
{
	// name
	"prev_events",

	// explanation
	R"(

	This column is deprecated and has been dropped from the schema. This
	descriptor will erase its presence in the database upon next open.

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
	true,
};

const ircd::db::descriptor
ircd::m::dbs::desc::events_prev_state
{
	// name
	"prev_state",

	// explanation
	R"(

	This column is deprecated and has been dropped from the schema. This
	descriptor will erase its presence in the database upon next open.

	)",

	// typing (key, value)
	{
		typeid(uint64_t), typeid(ircd::string_view)
	},

	// options
	{},

	// comparator
	{},

	// prefix transform
	{},

	// drop column
	true,
};

const ircd::db::descriptor
ircd::m::dbs::desc::events_redacts
{
	// name
	"redacts",

	// explanation
	R"(

	This column is deprecated and has been dropped from the schema. This
	descriptor will erase its presence in the database upon next open.

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
	true,
};

const ircd::db::descriptor
ircd::m::dbs::desc::events_signatures
{
	// name
	"signatures",

	// explanation
	R"(

	This column is deprecated and has been dropped from the schema. This
	descriptor will erase its presence in the database upon next open.

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
	true,
};

const ircd::db::descriptor
ircd::m::dbs::desc::events__default
{
	// name
	"default",

	// explanation
	R"(This column is unused but required by the database software.

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
	{},

	// drop column
	false,

	// cache size
	0_MiB,

	// cache size for compressed assets
	0_MiB,

	// bloom filter bits
	0,

	// expect queries hit
	false,
};

//
// Description vector
//

const ircd::db::description
ircd::m::dbs::desc::events
{
	// Requirement of RocksDB/LevelDB
	events__default,

	//
	// These columns directly represent event fields indexed by event_idx
	// number and the value is the actual event values. Some values may be
	// JSON, like content.
	//

	events_content,
	events_depth,
	events_event_id,
	events_origin,
	events_origin_server_ts,
	events_room_id,
	events_sender,
	events_state_key,
	events_type,

	//
	// These columns are metadata oriented around the event data.
	//

	// event_id => uint64_t
	// Mapping of event_id to index number.
	events__event_idx,

	// event_idx => json
	// Mapping of event_idx to full json
	events__event_json,

	// event_idx | event_idx
	// Reverse mapping of the event reference graph.
	events__event_refs,

	// event_idx | event_idx
	// Mapping of unresolved event refs.
	events__event_horizon,

	// origin | sender, event_idx
	// Mapping of senders to event_idx's they are the sender of.
	events__event_sender,

	// type | event_idx
	// Mapping of type strings to event_idx's of that type.
	events__event_type,

	// (room_id, (depth, event_idx)) => (state_root)
	// Sequence of all events for a room, ever.
	events__room_events,

	// (room_id, (origin, user_id)) => ()
	// Sequence of all PRESENTLY JOINED joined for a room.
	events__room_joined,

	// (room_id, (type, state_key)) => (event_idx)
	// Sequence of the PRESENT STATE of the room.
	events__room_state,

	// (room_id, (type, state_key, depth, event_idx))
	// Sequence of all states of the room.
	events__room_state_space,

	// (state tree node id) => (state tree node)
	// Mapping of state tree node id to node data.
	events__state_node,

	// (room_id, event_id) => (event_idx)
	// Mapping of all current head events for a room.
	events__room_head,

	//
	// These columns are legacy; they have been dropped from the schema.
	//

	events_auth_events,
	events_hashes,
	events_membership,
	events_prev_events,
	events_prev_state,
	events_redacts,
	events_signatures,
	events__event_auth,
	events__event_bad,
};
