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

/// Linkage for a reference to the event_bad column.
decltype(ircd::m::dbs::event_bad)
ircd::m::dbs::event_bad
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

//
// init
//

/// Initializes the m::dbs subsystem; sets up the events database. Held/called
/// by m::init. Most of the extern variables in m::dbs are not ready until
/// this call completes.
ircd::m::dbs::init::init(std::string dbopts)
{
	// Open the events database
	static const auto dbname{"events"};
	events = std::make_shared<database>(dbname, std::move(dbopts), desc::events);

	// Cache the columns for the event tuple in order for constant time lookup
	assert(event_columns == event::size());
	std::array<string_view, event::size()> keys;      //TODO: why did this happen?
	_key_transform(event{}, begin(keys), end(keys));  //TODO: how did this happen?
	for(size_t i(0); i < keys.size(); ++i)
		event_column[i] = db::column
		{
			*events, keys[i]
		};

	// Cache the columns for the metadata
	event_idx = db::column{*events, desc::events__event_idx.name};
	event_bad = db::column{*events, desc::events__event_bad.name};
	room_head = db::index{*events, desc::events__room_head.name};
	room_events = db::index{*events, desc::events__room_events.name};
	room_joined = db::index{*events, desc::events__room_joined.name};
	room_state = db::index{*events, desc::events__room_state.name};
	state_node = db::column{*events, desc::events__state_node.name};
}

/// Shuts down the m::dbs subsystem; closes the events database. The extern
/// variables in m::dbs will no longer be functioning after this call.
ircd::m::dbs::init::~init()
noexcept
{
	// Unref DB (should close)
	events = {};
}

//
// Basic write suite
//

void
ircd::m::dbs::blacklist(db::txn &txn,
                        const event::id &event_id,
                        const write_opts &opts)
{
	db::txn::append
	{
		txn, event_bad,
		{
			opts.op,
			string_view{event_id},
			opts.event_idx != uint64_t(-1) && opts.op == db::op::SET?
				byte_view<string_view>(opts.event_idx):
				string_view{}
		}
	};
}

ircd::string_view
ircd::m::dbs::write(db::txn &txn,
                    const event &event,
                    const write_opts &opts)
{
	if(unlikely(opts.event_idx == 0))
		throw ircd::error
		{
			"Cannot write to database: no index specified for event."
		};

	// event_idx
	if(opts.indexer)
		_index__event(txn, event, opts);

	// Direct columns
	db::txn::append
	{
		txn, byte_view<string_view>(opts.event_idx), event, event_column, opts.op
	};

	if(opts.head || opts.refs)
		_index__room_head(txn, event, opts);

	if(defined(json::get<"state_key"_>(event)))
		return _index_state(txn, event, opts);

	if(at<"type"_>(event) == "m.room.redaction")
		return _index_redact(txn, event, opts);

	return _index_ephem(txn, event, opts);
}

//
// Internal interface
//

void
ircd::m::dbs::_index__event(db::txn &txn,
                            const event &event,
                            const write_opts &opts)
{
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

ircd::string_view
ircd::m::dbs::_index_ephem(db::txn &txn,
                           const event &event,
                           const write_opts &opts)
{
	_index__room_events(txn, event, opts, opts.root_in);
	return strlcpy(opts.root_out, opts.root_in);
}

ircd::string_view
ircd::m::dbs::_index_redact(db::txn &txn,
                            const event &event,
                            const write_opts &opts)
try
{
	const auto &target_id
	{
		at<"redacts"_>(event)
	};

	event::fetch target
	{
		target_id, std::nothrow
	};

	if(unlikely(!target.valid))
		log::error
		{
			"Redaction from '%s' missing redaction target '%s'",
			at<"event_id"_>(event),
			target_id
		};

	if(!defined(json::get<"state_key"_>(target)))
	{
		_index__room_events(txn, event, opts, opts.root_in);
		return strlcpy(opts.root_out, opts.root_in);
	}

	const string_view new_root
	{
		//state::remove(txn, state_root_out, state_root_in, target)
		strlcpy(opts.root_out, opts.root_in)
	};

	_index__room_events(txn, event, opts, new_root);
	return new_root;
}
catch(const std::exception &e)
{
	log::error
	{
		"Failed to update state from redaction: %s", e.what()
	};

	throw;
}

ircd::string_view
ircd::m::dbs::_index_state(db::txn &txn,
                           const event &event,
                           const write_opts &opts)
try
{
	const auto &type
	{
		at<"type"_>(event)
	};

	const auto &room_id
	{
		at<"room_id"_>(event)
	};

	const string_view &new_root
	{
		opts.op == db::op::SET && opts.history?
			state::insert(txn, opts.root_out, opts.root_in, event):
			strlcpy(opts.root_out, opts.root_in)
	};

	_index__room_events(txn, event, opts, new_root);
	_index__room_joined(txn, event, opts);
	_index__room_state(txn, event, opts);
	return new_root;
}
catch(const std::exception &e)
{
	log::error
	{
		"Failed to update state: %s", e.what()
	};

	throw;
}

void
ircd::m::dbs::_index__room_head(db::txn &txn,
                                const event &event,
                                const write_opts &opts)
{
	thread_local char buf[ROOM_HEAD_KEY_MAX_SIZE];
	const ctx::critical_assertion ca;

	if(opts.head)
	{
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

	//TODO: If op is DELETE and we are deleting this event and thereby
	//TODO: potentially creating a gap in the reference graph (just for us
	//TODO: though) can we *re-add* the prev_events to the head?

	if(opts.refs && opts.op == db::op::SET)
	{
		const m::event::prev &prev{event};
		for(const json::array &p : json::get<"prev_events"_>(prev))
		{
			const auto &event_id
			{
				unquote(p.at(0))
			};

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
}

/// Adds the entry for the room_events column into the txn.
/// You need find/create the right state_root before this.
void
ircd::m::dbs::_index__room_events(db::txn &txn,
                                  const event &event,
                                  const write_opts &opts,
                                  const string_view &new_root)
{
	const ctx::critical_assertion ca;
	thread_local char buf[ROOM_EVENTS_KEY_MAX_SIZE];
	const string_view &key
	{
		room_events_key(buf, at<"room_id"_>(event), at<"depth"_>(event), opts.event_idx)
	};

	db::txn::append
	{
		txn, room_events,
		{
			opts.op,   // db::op
			key,       // key
			new_root   // val
		}
	};
}

/// Adds the entry for the room_joined column into the txn.
/// This only is affected if opts.present=true
void
ircd::m::dbs::_index__room_joined(db::txn &txn,
                                   const event &event,
                                   const write_opts &opts)
{
	if(!opts.present)
		return;

	if(at<"type"_>(event) != "m.room.member")
		return;

	const ctx::critical_assertion ca;
	thread_local char buf[ROOM_JOINED_KEY_MAX_SIZE];
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

/// Adds the entry for the room_joined column into the txn.
/// This only is affected if opts.present=true
void
ircd::m::dbs::_index__room_state(db::txn &txn,
                                 const event &event,
                                 const write_opts &opts)
{
	if(!opts.present)
		return;

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

	const db::op op
	{
		opts.op == db::op::SET && at<"type"_>(event) != "m.room.redaction"?
			db::op::SET:
			db::op::DELETE
	};

	db::txn::append
	{
		txn, room_state,
		{
			op,
			key,
			value_required(op)? val : string_view{},
		}
	};
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

decltype(ircd::m::dbs::desc::events__event_idx__block__size)
ircd::m::dbs::desc::events__event_idx__block__size
{
	{ "name",     "ircd.m.dbs.events._event_idx.block.size" },
	{ "default",  512L                                      },
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

const ircd::db::descriptor
ircd::m::dbs::desc::events__event_idx
{
	// name
	"_event_idx",

	// explanation
	R"(### developer note:

	The key is an event_id and the value is the index number to be used as the
	key to all the event data columns.

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

	// cache size
	bool(events_cache_enable)? -1 : 0, //uses conf item

	// cache size for compressed assets
	bool(events_cache_comp_enable)? -1 : 0,

	// bloom filter bits
	10,

	// expect queries hit
	false,

	// block size
	size_t(events__event_idx__block__size),
};

decltype(ircd::m::dbs::desc::events__event_bad__cache__size)
ircd::m::dbs::desc::events__event_bad__cache__size
{
	{
		{ "name",     "ircd.m.dbs.events._event_bad.cache.size" },
		{ "default",  long(16_MiB)                              },
	}, []
	{
		const size_t &value{events__event_bad__cache__size};
		db::capacity(db::cache(event_bad), value);
	}
};

decltype(ircd::m::dbs::desc::events__event_bad__cache_comp__size)
ircd::m::dbs::desc::events__event_bad__cache_comp__size
{
	{
		{ "name",     "ircd.m.dbs.events._event_bad.cache_comp.size" },
		{ "default",  long(16_MiB)                                   },
	}, []
	{
		const size_t &value{events__event_bad__cache_comp__size};
		db::capacity(db::cache_compressed(event_bad), value);
	}
};

const ircd::db::descriptor
ircd::m::dbs::desc::events__event_bad
{
	// name
	"_event_bad",

	// explanation
	R"(### developer note:

	The key is an event_id. The mere existence of the key indicates we do not
	have this event; it will not be a key found in the event_idx and will not
	have its own idx number and we know nothing else about this event.
	
	However the value is an optional index number of *another* event which is
	recommended to be used when this missing event was essential. For example,
	if other servers accept the bad event and then continue a room DAG it is
	essential we know the last good prev reference to have an unbroken trace.
	Such a prev reference will be recorded in this value for said use.

	If this column has no value or a zero value it is being used to blacklist
	an event_id for some other reason.

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

	// cache size
	bool(events_cache_enable)? -1 : 0,

	// cache size for compressed assets
	bool(events_cache_comp_enable)? -1 : 0,

	// bloom filter bits
	10,

	// expect queries hit
	false,
};

//
// room_head
//

decltype(ircd::m::dbs::desc::events__room_head__block__size)
ircd::m::dbs::desc::events__room_head__block__size
{
	{ "name",     "ircd.m.dbs.events._room_head.block.size" },
	{ "default",  512L                                      },
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
/// [room_id | event_id => event_idx]
///
/// The key is composed of two parts: the room_id which forms a domain for
/// iteration, and the event_id of the unreferenced event.
///
/// The value is a gratuitious event_idx of the event_id in the key. This
/// allows foregoing the query for event_id=>event_idx when one wants to fetch
/// more data for the event.
///
/// The way this column works is simple: when the m::vm writes any event (that
/// means full valid acceptance) it will SET an entry into this column for it.
/// At that time the prev_events of that event will be iterated and DELETE
/// ops will be appended in the transaction.
///
const ircd::db::descriptor
ircd::m::dbs::desc::events__room_head
{
	// name
	"_room_head",

	// explanation
	R"(### developer note:

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

	// cache size
	bool(events_cache_enable)? -1 : 0,

	// cache size for compressed assets
	0, //no compresed cache

	// bloom filter bits
	0,

	// expect queries hit
	false,

	// block size
	size_t(events__room_head__block__size),
};

//
// room_events
//

decltype(ircd::m::dbs::desc::events__room_events__block__size)
ircd::m::dbs::desc::events__room_events__block__size
{
	{ "name",     "ircd.m.dbs.events._room_events.block.size" },
	{ "default",  8192L                                       },
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
	assert(size(amalgam) == 1 + 8 + 8 || size(amalgam) == 1 + 8);
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
	R"(### developer note:

	the prefix transform is in effect. this column indexes events by
	room_id offering an iterable bound of the index prefixed by room_id

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
};

//
// joined sequential
//

decltype(ircd::m::dbs::desc::events__room_joined__block__size)
ircd::m::dbs::desc::events__room_joined__block__size
{
	{ "name",     "ircd.m.dbs.events._room_joined.block.size" },
	{ "default",  4096L                                       },
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
	R"(### developer note:

	the prefix transform is in effect. this column indexes events by
	room_id offering an iterable bound of the index prefixed by room_id

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

	// cache size
	bool(events_cache_enable)? -1 : 0,

	// cache size for compressed assets
	bool(events_cache_comp_enable)? -1 : 0,

	// bloom filter bits
	10,

	// expect queries hit
	false,

	// block size
	size_t(events__room_joined__block__size),
};

//
// state sequential
//

decltype(ircd::m::dbs::desc::events__room_state__block__size)
ircd::m::dbs::desc::events__room_state__block__size
{
	{ "name",     "ircd.m.dbs.events._room_state.block.size" },
	{ "default",  8192L                                      },
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
                             const string_view &type,
                             const string_view &state_key)
{
	mutable_buffer out{out_};
	consume(out, copy(out, room_id));
	consume(out, copy(out, "\0"_sv));
	consume(out, copy(out, type));
	consume(out, copy(out, "\0"_sv));
	consume(out, copy(out, state_key));
	return { data(out_), data(out) };
}

ircd::string_view
ircd::m::dbs::room_state_key(const mutable_buffer &out_,
                             const id::room &room_id,
                             const string_view &type)
{
	mutable_buffer out{out_};
	consume(out, copy(out, room_id));
	consume(out, copy(out, "\0"_sv));
	consume(out, copy(out, type));
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
	R"(### developer note:

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

	// cache size
	bool(events_cache_enable)? -1 : 0,

	// cache size for compressed assets
	bool(events_cache_comp_enable)? -1 : 0,

	// bloom filter bits
	10,

	// expect queries hit
	false,

	// block size
	size_t(events__room_state__block__size),
};

//
// state node
//

decltype(ircd::m::dbs::desc::events__state_node__block__size)
ircd::m::dbs::desc::events__state_node__block__size
{
	{ "name",     "ircd.m.dbs.events._state_node.block.size" },
	{ "default",  512L                                       },
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
	R"(### developer note:

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

	// cache size
	bool(events_cache_enable)? -1 : 0,

	// cache size for compressed assets
	bool(events_cache_comp_enable)? -1 : 0,

	// bloom filter bits
	10,

	// expect queries hit
	true,

	// block size
	size_t(events__state_node__block__size),
};

//
// Direct column descriptors
//

//
// event_id
//

decltype(ircd::m::dbs::desc::events__event_id__block__size)
ircd::m::dbs::desc::events__event_id__block__size
{
	{ "name",     "ircd.m.dbs.events.event_id.block.size"  },
	{ "default",  512L                                     },
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
	R"(### protocol note:

	10.1
	The id of event.

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

	// cache size
	bool(events_cache_enable)? -1 : 0,

	// cache size for compressed assets
	bool(events_cache_comp_enable)? -1 : 0,

	// bloom filter bits
	10,

	// expect queries hit
	true,

	// block size
	size_t(events__event_id__block__size),
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
	R"(### protocol note:

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

	// cache size
	bool(events_cache_enable)? -1 : 0,

	// cache size for compressed assets
	bool(events_cache_comp_enable)? -1 : 0,

	// bloom filter bits
	10,

	// expect queries hit
	true,

	// block size
	size_t(events__type__block__size),
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
	R"(### protocol note:

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

	// cache size
	bool(events_cache_enable)? -1 : 0,

	// cache size for compressed assets
	bool(events_cache_comp_enable)? -1 : 0,

	// bloom filter bits
	10,

	// expect queries hit
	true,

	// block size
	size_t(events__content__block__size),
};

//
// redacts
//

decltype(ircd::m::dbs::desc::events__redacts__block__size)
ircd::m::dbs::desc::events__redacts__block__size
{
	{ "name",     "ircd.m.dbs.events.redacts.block.size"  },
	{ "default",  512L                                    },
};

decltype(ircd::m::dbs::desc::events__redacts__cache__size)
ircd::m::dbs::desc::events__redacts__cache__size
{
	{
		{ "name",     "ircd.m.dbs.events.redacts.cache.size"  },
		{ "default",  long(8_MiB)                             },
	}, []
	{
		auto &column(event_column.at(json::indexof<event, "redacts"_>()));
		const size_t &value{events__redacts__cache__size};
		db::capacity(db::cache(column), value);
	}
};

decltype(ircd::m::dbs::desc::events__redacts__cache_comp__size)
ircd::m::dbs::desc::events__redacts__cache_comp__size
{
	{
		{ "name",     "ircd.m.dbs.events.redacts.cache_comp.size"  },
		{ "default",  long(4_MiB)                                  },
	}, []
	{
		auto &column(event_column.at(json::indexof<event, "redacts"_>()));
		const size_t &value{events__redacts__cache_comp__size};
		db::capacity(db::cache_compressed(column), value);
	}
};

const ircd::db::descriptor
ircd::m::dbs::desc::events_redacts
{
	// name
	"redacts",

	// explanation
	R"(### protocol note:

	### developer note:
	key is event_idx number.
	value is targeted event_id
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

	// cache size
	bool(events_cache_enable)? -1 : 0,

	// cache size for compressed assets
	bool(events_cache_comp_enable)? -1 : 0,

	// bloom filter bits
	10,

	// expect queries hit
	false,

	// block size
	size_t(events__redacts__block__size),
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
	R"(### protocol note:

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

	// cache size
	bool(events_cache_enable)? -1 : 0,

	// cache size for compressed assets
	bool(events_cache_comp_enable)? -1 : 0,

	// bloom filter bits
	10,

	// expect queries hit
	true,

	// block size
	size_t(events__room_id__block__size),
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
	R"(### protocol note:

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

	// cache size
	bool(events_cache_enable)? -1 : 0,

	// cache size for compressed assets
	bool(events_cache_comp_enable)? -1 : 0,

	// bloom filter bits
	10,

	// expect queries hit
	true,

	// block size
	size_t(events__sender__block__size),
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
	R"(### protocol note:

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

	// cache size
	bool(events_cache_enable)? -1 : 0,

	// cache size for compressed assets
	bool(events_cache_comp_enable)? -1 : 0,

	// bloom filter bits
	10,

	// expect queries hit
	false,

	// block size
	size_t(events__state_key__block__size),
};

//
// origin
//

decltype(ircd::m::dbs::desc::events__origin__block__size)
ircd::m::dbs::desc::events__origin__block__size
{
	{ "name",     "ircd.m.dbs.events.origin.block.size"  },
	{ "default",  512L                                   },
};

decltype(ircd::m::dbs::desc::events__origin__cache__size)
ircd::m::dbs::desc::events__origin__cache__size
{
	{
		{ "name",     "ircd.m.dbs.events.origin.cache.size"  },
		{ "default",  long(16_MiB)                           },
	}, []
	{
		auto &column(event_column.at(json::indexof<event, "origin"_>()));
		const size_t &value{events__origin__cache__size};
		db::capacity(db::cache(column), value);
	}
};

decltype(ircd::m::dbs::desc::events__origin__cache_comp__size)
ircd::m::dbs::desc::events__origin__cache_comp__size
{
	{
		{ "name",     "ircd.m.dbs.events.origin.cache_comp.size"  },
		{ "default",  long(16_MiB)                                },
	}, []
	{
		auto &column(event_column.at(json::indexof<event, "origin"_>()));
		const size_t &value{events__origin__cache_comp__size};
		db::capacity(db::cache_compressed(column), value);
	}
};

const ircd::db::descriptor
ircd::m::dbs::desc::events_origin
{
	// name
	"origin",

	// explanation
	R"(### protocol note:

	FEDERATION 4.1
	DNS name of homeserver that created this PDU

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

	// cache size
	bool(events_cache_enable)? -1 : 0,

	// cache size for compressed assets
	bool(events_cache_comp_enable)? -1 : 0,

	// bloom filter bits
	10,

	// expect queries hit
	true,

	// block size
	size_t(events__origin__block__size),
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
	R"(### protocol note:

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

	// cache size
	bool(events_cache_enable)? -1 : 0,

	// cache size for compressed assets
	bool(events_cache_comp_enable)? -1 : 0,

	// bloom filter bits
	10,

	// expect queries hit
	true,

	// block size
	size_t(events__origin_server_ts__block__size),
};

//
// signatures
//

decltype(ircd::m::dbs::desc::events__signatures__block__size)
ircd::m::dbs::desc::events__signatures__block__size
{
	{ "name",     "ircd.m.dbs.events.signatures.block.size"  },
	{ "default",  512L                                       },
};

decltype(ircd::m::dbs::desc::events__signatures__cache__size)
ircd::m::dbs::desc::events__signatures__cache__size
{
	{
		{ "name",     "ircd.m.dbs.events.signatures.cache.size"  },
		{ "default",  long(16_MiB)                               },
	}, []
	{
		auto &column(event_column.at(json::indexof<event, "signatures"_>()));
		const size_t &value{events__signatures__cache__size};
		db::capacity(db::cache(column), value);
	}
};

decltype(ircd::m::dbs::desc::events__signatures__cache_comp__size)
ircd::m::dbs::desc::events__signatures__cache_comp__size
{
	{
		{ "name",     "ircd.m.dbs.events.signatures.cache_comp.size"  },
		{ "default",  long(16_MiB)                                    },
	}, []
	{
		auto &column(event_column.at(json::indexof<event, "signatures"_>()));
		const size_t &value{events__signatures__cache_comp__size};
		db::capacity(db::cache_compressed(column), value);
	}
};

const ircd::db::descriptor
ircd::m::dbs::desc::events_signatures
{
	// name
	"signatures",

	// explanation
	R"(### protocol note:

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

	// cache size
	bool(events_cache_enable)? -1 : 0,

	// cache size for compressed assets
	bool(events_cache_comp_enable)? -1 : 0,

	// bloom filter bits
	10,

	// expect queries hit
	true,

	// block size
	size_t(events__signatures__block__size),
};

//
// auth_events
//

decltype(ircd::m::dbs::desc::events__auth_events__block__size)
ircd::m::dbs::desc::events__auth_events__block__size
{
	{ "name",     "ircd.m.dbs.events.auth_events.block.size"  },
	{ "default",  512L                                        },
};

decltype(ircd::m::dbs::desc::events__auth_events__cache__size)
ircd::m::dbs::desc::events__auth_events__cache__size
{
	{
		{ "name",     "ircd.m.dbs.events.auth_events.cache.size"  },
		{ "default",  long(16_MiB)                                },
	}, []
	{
		auto &column(event_column.at(json::indexof<event, "auth_events"_>()));
		const size_t &value{events__auth_events__cache__size};
		db::capacity(db::cache(column), value);
	}
};

decltype(ircd::m::dbs::desc::events__auth_events__cache_comp__size)
ircd::m::dbs::desc::events__auth_events__cache_comp__size
{
	{
		{ "name",     "ircd.m.dbs.events.auth_events.cache_comp.size"  },
		{ "default",  long(16_MiB)                                     },
	}, []
	{
		auto &column(event_column.at(json::indexof<event, "auth_events"_>()));
		const size_t &value{events__auth_events__cache_comp__size};
		db::capacity(db::cache_compressed(column), value);
	}
};

const ircd::db::descriptor
ircd::m::dbs::desc::events_auth_events
{
	// name
	"auth_events",

	// explanation
	R"(### protocol note:

	### developer note:
	key is event_idx number..
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

	// cache size
	bool(events_cache_enable)? -1 : 0,

	// cache size for compressed assets
	bool(events_cache_comp_enable)? -1 : 0,

	// bloom filter bits
	10,

	// expect queries hit
	false,

	// block size
	size_t(events__auth_events__block__size),
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
	R"(### protocol note:

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

	// cache size
	bool(events_cache_enable)? -1 : 0,

	// cache size for compressed assets
	bool(events_cache_comp_enable)? -1 : 0,

	// bloom filter bits
	10,

	// expect queries hit
	true,

	// block size
	size_t(events__depth__block__size),
};

//
// hashes
//

decltype(ircd::m::dbs::desc::events__hashes__block__size)
ircd::m::dbs::desc::events__hashes__block__size
{
	{ "name",     "ircd.m.dbs.events.hashes.block.size"  },
	{ "default",  512L                                   },
};

decltype(ircd::m::dbs::desc::events__hashes__cache__size)
ircd::m::dbs::desc::events__hashes__cache__size
{
	{
		{ "name",     "ircd.m.dbs.events.hashes.cache.size"  },
		{ "default",  long(16_MiB)                           },
	}, []
	{
		auto &column(event_column.at(json::indexof<event, "hashes"_>()));
		const size_t &value{events__hashes__cache__size};
		db::capacity(db::cache(column), value);
	}
};

decltype(ircd::m::dbs::desc::events__hashes__cache_comp__size)
ircd::m::dbs::desc::events__hashes__cache_comp__size
{
	{
		{ "name",     "ircd.m.dbs.events.hashes.cache_comp.size"  },
		{ "default",  long(16_MiB)                                },
	}, []
	{
		auto &column(event_column.at(json::indexof<event, "hashes"_>()));
		const size_t &value{events__hashes__cache_comp__size};
		db::capacity(db::cache_compressed(column), value);
	}
};

const ircd::db::descriptor
ircd::m::dbs::desc::events_hashes
{
	// name
	"hashes",

	// explanation
	R"(### protocol note:

	### developer note:
	key is event_idx number..
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

	// cache size
	bool(events_cache_enable)? -1 : 0,

	// cache size for compressed assets
	bool(events_cache_comp_enable)? -1 : 0,

	// bloom filter bits
	10,

	// expect queries hit
	true,

	// block size
	size_t(events__hashes__block__size),
};

//
// membership
//

decltype(ircd::m::dbs::desc::events__membership__block__size)
ircd::m::dbs::desc::events__membership__block__size
{
	{ "name",     "ircd.m.dbs.events.membership.block.size"  },
	{ "default",  256L                                       },
};

decltype(ircd::m::dbs::desc::events__membership__cache__size)
ircd::m::dbs::desc::events__membership__cache__size
{
	{
		{ "name",     "ircd.m.dbs.events.membership.cache.size"  },
		{ "default",  long(8_MiB)                                },
	}, []
	{
		auto &column(event_column.at(json::indexof<event, "membership"_>()));
		const size_t &value{events__membership__cache__size};
		db::capacity(db::cache(column), value);
	}
};

decltype(ircd::m::dbs::desc::events__membership__cache_comp__size)
ircd::m::dbs::desc::events__membership__cache_comp__size
{
	{
		{ "name",     "ircd.m.dbs.events.membership.cache_comp.size"  },
		{ "default",  long(16_MiB)                                    },
	}, []
	{
		auto &column(event_column.at(json::indexof<event, "membership"_>()));
		const size_t &value{events__membership__cache_comp__size};
		db::capacity(db::cache_compressed(column), value);
	}
};

const ircd::db::descriptor
ircd::m::dbs::desc::events_membership
{
	// name
	"membership",

	// explanation
	R"(### protocol note:

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

	// cache size
	bool(events_cache_enable)? -1 : 0,

	// cache size for compressed assets
	bool(events_cache_comp_enable)? -1 : 0,

	// bloom filter bits
	10,

	// expect queries hit
	false,

	// block size
	size_t(events__membership__block__size),
};

//
// prev_events
//

decltype(ircd::m::dbs::desc::events__prev_events__block__size)
ircd::m::dbs::desc::events__prev_events__block__size
{
	{ "name",     "ircd.m.dbs.events.prev_events.block.size"  },
	{ "default",  512L                                        },
};

decltype(ircd::m::dbs::desc::events__prev_events__cache__size)
ircd::m::dbs::desc::events__prev_events__cache__size
{
	{
		{ "name",     "ircd.m.dbs.events.prev_events.cache.size"  },
		{ "default",  long(16_MiB)                                },
	}, []
	{
		auto &column(event_column.at(json::indexof<event, "prev_events"_>()));
		const size_t &value{events__prev_events__cache__size};
		db::capacity(db::cache(column), value);
	}
};

decltype(ircd::m::dbs::desc::events__prev_events__cache_comp__size)
ircd::m::dbs::desc::events__prev_events__cache_comp__size
{
	{
		{ "name",     "ircd.m.dbs.events.prev_events.cache_comp.size"  },
		{ "default",  long(16_MiB)                                     },
	}, []
	{
		auto &column(event_column.at(json::indexof<event, "prev_events"_>()));
		const size_t &value{events__prev_events__cache_comp__size};
		db::capacity(db::cache_compressed(column), value);
	}
};

const ircd::db::descriptor
ircd::m::dbs::desc::events_prev_events
{
	// name
	"prev_events",

	// explanation
	R"(### protocol note:

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

	// cache size
	bool(events_cache_enable)? -1 : 0,

	// cache size for compressed assets
	bool(events_cache_comp_enable)? -1 : 0,

	// bloom filter bits
	10,

	// expect queries hit
	true,

	// block size
	size_t(events__prev_events__block__size),
};

//
// prev_state
//

decltype(ircd::m::dbs::desc::events__prev_state__block__size)
ircd::m::dbs::desc::events__prev_state__block__size
{
	{ "name",     "ircd.m.dbs.events.prev_state.block.size"  },
	{ "default",  256L                                       },
};

decltype(ircd::m::dbs::desc::events__prev_state__cache__size)
ircd::m::dbs::desc::events__prev_state__cache__size
{
	{
		{ "name",     "ircd.m.dbs.events.prev_state.cache.size"  },
		{ "default",  long(16_MiB)                               },
	}, []
	{
		auto &column(event_column.at(json::indexof<event, "prev_state"_>()));
		const size_t &value{events__prev_state__cache__size};
		db::capacity(db::cache(column), value);
	}
};

decltype(ircd::m::dbs::desc::events__prev_state__cache_comp__size)
ircd::m::dbs::desc::events__prev_state__cache_comp__size
{
	{
		{ "name",     "ircd.m.dbs.events.prev_state.cache_comp.size"  },
		{ "default",  long(16_MiB)                                    },
	}, []
	{
		auto &column(event_column.at(json::indexof<event, "prev_state"_>()));
		const size_t &value{events__prev_state__cache_comp__size};
		db::capacity(db::cache_compressed(column), value);
	}
};

const ircd::db::descriptor
ircd::m::dbs::desc::events_prev_state
{
	// name
	"prev_state",

	// explanation
	R"(### protocol note:

	### developer note:
	key is event_idx number.
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

	// cache size
	bool(events_cache_enable)? -1 : 0,

	// cache size for compressed assets
	bool(events_cache_comp_enable)? -1 : 0,

	// bloom filter bits
	10,

	// expect queries hit
	false,

	// block size
	size_t(events__prev_state__block__size),
};

namespace ircd::m::dbs::desc
{
	extern const ircd::db::descriptor events__default;
};

const ircd::db::descriptor
ircd::m::dbs::desc::events__default
{
	// name
	"default",

	// explanation
	R"(
		This column is unused but required.
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

	// cache size
	0_MiB,

	// cache size for compressed assets
	0_MiB,

	// bloom filter bits
	0,

	// expect queries hit
	false,
};

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

	events_auth_events,
	events_content,
	events_depth,
	events_event_id,
	events_hashes,
	events_membership,
	events_origin,
	events_origin_server_ts,
	events_prev_events,
	events_prev_state,
	events_redacts,
	events_room_id,
	events_sender,
	events_signatures,
	events_state_key,
	events_type,

	//
	// These columns are metadata oriented around the event data.
	//

	// event_id => uint64_t
	// Mapping of event_id to index number.
	events__event_idx,

	// (room_id, (depth, event_idx)) => (state_root)
	// Sequence of all events for a room, ever.
	events__room_events,

	// (room_id, (origin, user_id)) => ()
	// Sequence of all PRESENTLY JOINED joined for a room.
	events__room_joined,

	// (room_id, (type, state_key)) => (event_id)
	// Sequence of the PRESENT STATE of the room.
	events__room_state,

	// (state tree node id) => (state tree node)
	// Mapping of state tree node id to node data.
	events__state_node,

	// event_id => uint64_t
	// Mapping of faulty event_id to possible alternative event_idx.
	events__event_bad,

	// (room_id, event_id) => (event_idx)
	// Mapping of all current head events for a room.
	events__room_head,
};
