// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <ircd/m/m.h>

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

/// Linkage for a reference to the state_node column.
decltype(ircd::m::dbs::state_node)
ircd::m::dbs::state_node
{};

/// Linkage for a reference to the room_events column
decltype(ircd::m::dbs::room_events)
ircd::m::dbs::room_events
{};

/// Linkage for a reference to the room_origins column
decltype(ircd::m::dbs::room_origins)
ircd::m::dbs::room_origins
{};

/// Linkage for a reference to the room_state column
decltype(ircd::m::dbs::room_state)
ircd::m::dbs::room_state
{};

//
// init
//

/// Initializes the m::dbs subsystem; sets up the events database. Held/called
/// by m::init. Most of the extern variables in m::dbs are not ready until
/// this call completes.
ircd::m::dbs::init::init()
{
	// Open the events database
	events = std::make_shared<database>("events"s, ""s, desc::events);

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
	state_node = db::column{*events, "_state_node"};
	room_events = db::index{*events, "_room_events"};
	room_origins = db::index{*events, "_room_origins"};
	room_state = db::index{*events, "_room_state"};
}

/// Shuts down the m::dbs subsystem; closes the events database. The extern
/// variables in m::dbs will no longer be functioning after this call.
ircd::m::dbs::init::~init()
noexcept
{
	// Columns should be unrefed before DB closes
	state_node = {};
	room_events = {};
	room_origins = {};
	room_state = {};
	for(auto &column : event_column)
		column = {};

	// Unref DB (should close)
	events = {};
}

namespace ircd::m::dbs
{
	static void _index__room_state(db::txn &,  const event &, const write_opts &);
	static void _index__room_events(db::txn &,  const event &, const write_opts &, const string_view &);
	static void _index__room_origins(db::txn &, const event &, const write_opts &);
	static string_view _index_state(db::txn &, const event &, const write_opts &);
	static string_view _index_redact(db::txn &, const event &, const write_opts &);
	static string_view _index_ephem(db::txn &, const event &, const write_opts &);
}

ircd::string_view
ircd::m::dbs::write(db::txn &txn,
                    const event &event,
                    const write_opts &opts)
{
	db::txn::append
	{
		txn, at<"event_id"_>(event), event
	};

	if(defined(json::get<"state_key"_>(event)))
		return _index_state(txn, event, opts);

	if(at<"type"_>(event) == "m.room.redaction")
		return _index_redact(txn, event, opts);

	return _index_ephem(txn, event, opts);
}

ircd::string_view
ircd::m::dbs::_index_ephem(db::txn &txn,
                           const event &event,
                           const write_opts &opts)
{
	_index__room_events(txn, event, opts, opts.root_in);
	return opts.root_in;
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

	if(unlikely(!target.valid(target_id)))
		log::error
		{
			"Redaction from '%s' missing redaction target '%s'",
			at<"event_id"_>(event),
			target_id
		};

	if(!defined(json::get<"state_key"_>(target)))
	{
		_index__room_events(txn, event, opts, opts.root_in);
		return opts.root_in;
	}

	const string_view new_root
	{
		opts.root_in //state::remove(txn, state_root_out, state_root_in, target)
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
		opts.history?
			state::insert(txn, opts.root_out, opts.root_in, event):
			opts.root_in
	};

	_index__room_events(txn, event, opts, new_root);
	_index__room_origins(txn, event, opts);
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

/// Adds the entry for the room_events column into the txn.
/// You need find/create the right state_root before this.
void
ircd::m::dbs::_index__room_events(db::txn &txn,
                                  const event &event,
                                  const write_opts &opts,
                                  const string_view &new_root)
{
	const ctx::critical_assertion ca;
	thread_local char buf[768];
	const string_view &key
	{
		room_events_key(buf, at<"room_id"_>(event), at<"depth"_>(event), at<"event_id"_>(event))
	};

	db::txn::append
	{
		txn, room_events,
		{
			key,       // key
			new_root   // val
		}
	};
}

/// Adds the entry for the room_origins column into the txn.
/// This only is affected if opts.present=true
void
ircd::m::dbs::_index__room_origins(db::txn &txn,
                                   const event &event,
                                   const write_opts &opts)
{
	if(!opts.present)
		return;

	if(at<"type"_>(event) != "m.room.member")
		return;

	const ctx::critical_assertion ca;
	thread_local char buf[512];
	const string_view &key
	{
		room_origins_key(buf, at<"room_id"_>(event), at<"origin"_>(event), at<"state_key"_>(event))
	};

	const string_view &membership
	{
		json::get<"membership"_>(event)?
			string_view{json::get<"membership"_>(event)}:
			unquote(at<"content"_>(event).get("membership"))
	};

	assert(!empty(membership));

	db::op op; switch(hash(membership))
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
	};

	db::txn::append
	{
		txn, room_origins,
		{
			op,
			key,
		}
	};
}

/// Adds the entry for the room_origins column into the txn.
/// This only is affected if opts.present=true
void
ircd::m::dbs::_index__room_state(db::txn &txn,
                                 const event &event,
                                 const write_opts &opts)
{
	if(!opts.present)
		return;

	const ctx::critical_assertion ca;
	thread_local char buf[768];
	const string_view &key
	{
		room_state_key(buf, at<"room_id"_>(event), at<"type"_>(event), at<"state_key"_>(event))
	};

	const string_view &val
	{
		at<"event_id"_>(event)
	};

	const db::op op
	{
		at<"type"_>(event) != "m.room.redaction"?
			db::op::SET:
			db::op::DELETE
	};

	db::txn::append
	{
		txn, room_state,
		{
			op,
			key,
			val,  // should be ignored on DELETE
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
	static constexpr auto idx
	{
		json::indexof<event, "room_id"_>()
	};

	auto &column
	{
		event_column.at(idx)
	};

	id::room::buf room_id;
	column(event_id, [&room_id](const string_view &val)
	{
		room_id = val;
	});

	return state_root(out, room_id, event_id);
}

ircd::string_view
ircd::m::dbs::state_root(const mutable_buffer &out,
                         const id::room &room_id,
                         const id::event &event_id)
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
	column(event_id, [&](const string_view &binary)
	{
		assert(size(binary) == sizeof(depth));
		depth = byte_view<uint64_t>(binary);
	});

	return state_root(out, room_id, event_id, depth);
}

ircd::string_view
ircd::m::dbs::state_root(const mutable_buffer &out,
                         const id::room &room_id,
                         const id::event &event_id,
                         const uint64_t &depth)
{
	char keybuf[768]; const auto key
	{
		room_events_key(keybuf, room_id, depth, event_id)
	};

	string_view ret;
	room_events(key, [&out, &ret](const string_view &val)
	{
		ret = { data(out), copy(out, val) };
	});

	return ret;
}

bool
ircd::m::dbs::exists(const event::id &event_id)
{
	static constexpr auto idx
	{
		json::indexof<event, "event_id"_>()
	};

	auto &column
	{
		event_column.at(idx)
	};

	return has(column, event_id);
}

//
// Database descriptors
//

/// State nodes are pieces of the m::state:: b-tree. The key is the hash
/// of the value, which serves as the ID of the node when referenced in
/// the tree. see: m/state.h for details.
///
const ircd::database::descriptor
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
};

/// Prefix transform for the events__room_events. The prefix here is a room_id
/// and the suffix is the depth+event_id concatenation.
/// for efficient sequences
///
/// TODO: This needs The Grammar
///
const ircd::db::prefix_transform
ircd::m::dbs::desc::events__room_events__pfx
{
	"_room_events",

	[](const string_view &key)
	{
		return key.find(":::") != key.npos;
	},

	[](const string_view &key)
	{
		return rsplit(key, ":::").first;
	}
};

/// Comparator for the events__room_events. The goal here is to sort the
/// events within a room by their depth from highest to lowest, so the
/// highest depth is hit first when a room is sought from this column.
///
/// TODO: This needs The Grammar
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

		if(pre[0] != pre[1])
			return pre[0] < pre[1];

		// After the prefix is the depth + event_id
		const string_view post[2]
		{
			a.substr(size(pre[0])),
			b.substr(size(pre[1])),
		};

		if(empty(post[0]))
			return true;

		if(empty(post[1]))
			return false;

		// Now want just the depth...
		const string_view depths[2]
		{
			between(post[0], ":::", "$"),
			between(post[1], ":::", "$"),
		};

		// ...as machine words
		const int64_t depth[2]
		{
			lex_cast<int64_t>(depths[0]),
			lex_cast<int64_t>(depths[1]),
		};

		// Highest to lowest sort so highest is first
		return depth[1] < depth[0];
	},

	// equal
	[](const string_view &a, const string_view &b)
	{
		return a == b;
	}
};

//TODO: optimize
//TODO: Needs The Gramslam
ircd::string_view
ircd::m::dbs::room_events_key(const mutable_buffer &out,
                              const id::room &room_id,
                              const uint64_t &depth)
{
	size_t len{0};
	len = strlcpy(out, room_id);
	len = strlcat(out, ":::");
	len = strlcat(out, lex_cast(depth));
	return { data(out), len };
}

//TODO: optimize
//TODO: Needs The Gramslam
ircd::string_view
ircd::m::dbs::room_events_key(const mutable_buffer &out,
                              const id::room &room_id,
                              const uint64_t &depth,
                              const id::event &event_id)
{
	size_t len{0};
	len = strlcpy(out, room_id);
	len = strlcat(out, ":::");
	len = strlcat(out, lex_cast(depth));
	len = strlcat(out, event_id);
	return { data(out), len };
}

std::tuple<uint64_t, ircd::string_view>
ircd::m::dbs::room_events_key(const string_view &amalgam)
{
	const auto depth
	{
		between(amalgam, ":::", "$")
	};

	return
	{
		lex_cast<uint64_t>(depth),
		string_view{end(depth), end(amalgam)}
	};
}

/// This column stores events in sequence in a room. Consider the following:
///
/// [room_id | depth + event_id => state_root]
///
/// The key is composed from three parts:
///
/// - `room_id` is the official prefix, bounding the sequence. That means we
/// make a blind query with just a room_id and get to the beginning of the
/// sequence, then iterate until we stop before the next room_id (upper bound).
///
/// - `depth` is the ordering. Within the sequence, all elements are ordered by
/// depth from HIGHEST TO LOWEST. The sequence will start at the highest depth.
///
/// - `event_id` is the key suffix. This column serves to sequence all events
/// within a room ordered by depth. There may be duplicate room_id|depth
/// prefixing but the event_id suffix gives the key total uniqueness.
///
/// The value is then used to store the node ID of the state tree root at this
/// event. Nodes of the state tree are stored in the state_node column. From
/// that root node the state of the room at the time of this event_id can be
/// queried.
///
/// There is one caveat here: we can't directly take a room_id and an event_id
/// and make a trivial query to find the state root, since the depth number
/// gets in the way. Rather than creating yet another column without the depth,
/// for the time being, we pay the cost of an extra query to events_depth and
/// find that missing piece to make the exact query with all three key parts.
///
const ircd::database::descriptor
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
		typeid(ircd::string_view), typeid(ircd::string_view)
	},

	// options
	{},

	// comparator
	events__room_events__cmp,

	// prefix transform
	events__room_events__pfx,
};

//
// origins sequential
//

/// Prefix transform for the events__room_origins
///
/// TODO: This needs The Grammar
///
const ircd::db::prefix_transform
ircd::m::dbs::desc::events__room_origins__pfx
{
	"_room_origins",

	[](const string_view &key)
	{
		return key.find(":::") != key.npos;
	},

	[](const string_view &key)
	{
		return rsplit(key, ":::").first;
	}
};

//TODO: optimize
//TODO: Needs The Gramslam
ircd::string_view
ircd::m::dbs::room_origins_key(const mutable_buffer &out,
                               const id::room &room_id,
                               const string_view &origin,
                               const id::user &member)
{
	size_t len{0};
	len = strlcpy(out, room_id);
	len = strlcat(out, ":::");
	len = strlcat(out, origin);
	len = strlcat(out, member);
	return { data(out), len };
}

std::tuple<ircd::string_view, ircd::string_view>
ircd::m::dbs::room_origins_key(const string_view &amalgam)
{
	const auto &key
	{
		lstrip(amalgam, ":::")
	};

	const auto &s
	{
		split(key, "@")
	};

	return
	{
		{ s.first },
		{ end(s.first), end(key) }
	};
}

const ircd::database::descriptor
ircd::m::dbs::desc::events__room_origins
{
	// name
	"_room_origins",

	// explanation
	R"(### developer note:

	the prefix transform is in effect. this column indexes events by
	room_id offering an iterable bound of the index prefixed by room_id

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
	events__room_origins__pfx,
};

//
// state sequential
//

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
		return key.find("\0"_sv) != key.npos;
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

std::tuple<ircd::string_view, ircd::string_view>
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

const ircd::database::descriptor
ircd::m::dbs::desc::events__room_state
{
	// name
	"_room_state",

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
	events__room_state__pfx,
};

//
// Direct column descriptors
//

const ircd::database::descriptor
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
	key is event_id. This is redundant data but we have to have it for now.
	)",

	// typing (key, value)
	{
		typeid(ircd::string_view), typeid(ircd::string_view)
	}
};

const ircd::database::descriptor
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
	key is event_id
	)",

	// typing (key, value)
	{
		typeid(ircd::string_view), typeid(ircd::string_view)
	}
};

const ircd::database::descriptor
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

	key is event_id
	)",

	// typing (key, value)
	{
		typeid(ircd::string_view), typeid(ircd::string_view)
	}
};

const ircd::database::descriptor
ircd::m::dbs::desc::events_redacts
{
	// name
	"redacts",

	// explanation
	R"(### protocol note:

	### developer note:
	key is event_id
	value is targeted event_id
	)",

	// typing (key, value)
	{
		typeid(ircd::string_view), typeid(ircd::string_view)
	}
};

const ircd::database::descriptor
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
	key is event_id
	)",

	// typing (key, value)
	{
		typeid(ircd::string_view), typeid(ircd::string_view)
	}
};

const ircd::database::descriptor
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
	key is event_id
	)",

	// typing (key, value)
	{
		typeid(ircd::string_view), typeid(ircd::string_view)
	}
};

const ircd::database::descriptor
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
	key is event_id
	)",

	// typing (key, value)
	{
		typeid(ircd::string_view), typeid(ircd::string_view)
	}
};

const ircd::database::descriptor
ircd::m::dbs::desc::events_origin
{
	// name
	"origin",

	// explanation
	R"(### protocol note:

	FEDERATION 4.1
	DNS name of homeserver that created this PDU

	### developer note:
	key is event_id
	)",

	// typing (key, value)
	{
		typeid(ircd::string_view), typeid(ircd::string_view)
	}
};

const ircd::database::descriptor
ircd::m::dbs::desc::events_origin_server_ts
{
	// name
	"origin_server_ts",

	// explanation
	R"(### protocol note:

	FEDERATION 4.1
	Timestamp in milliseconds on origin homeserver when this PDU was created.

	### developer note:
	key is event_id
	value is a machine integer (binary)

	TODO: consider unsigned rather than time_t because of millisecond precision

	)",

	// typing (key, value)
	{
		typeid(ircd::string_view), typeid(time_t)
	}
};

const ircd::database::descriptor
ircd::m::dbs::desc::events_signatures
{
	// name
	"signatures",

	// explanation
	R"(### protocol note:

	### developer note:
	key is event_id

	)",

	// typing (key, value)
	{
		typeid(ircd::string_view), typeid(ircd::string_view)
	}
};

const ircd::database::descriptor
ircd::m::dbs::desc::events_auth_events
{
	// name
	"auth_events",

	// explanation
	R"(### protocol note:

	### developer note:
	key is event_id.
	)",

	// typing (key, value)
	{
		typeid(ircd::string_view), typeid(ircd::string_view)
	}
};

const ircd::database::descriptor
ircd::m::dbs::desc::events_depth
{
	// name
	"depth",

	// explanation
	R"(### protocol note:

	### developer note:
	key is event_id value is long integer
	)",

	// typing (key, value)
	{
		typeid(ircd::string_view), typeid(int64_t)
	}
};

const ircd::database::descriptor
ircd::m::dbs::desc::events_hashes
{
	// name
	"hashes",

	// explanation
	R"(### protocol note:

	### developer note:
	key is event_id.
	)",

	// typing (key, value)
	{
		typeid(ircd::string_view), typeid(ircd::string_view)
	}
};

const ircd::database::descriptor
ircd::m::dbs::desc::events_membership
{
	// name
	"membership",

	// explanation
	R"(### protocol note:

	### developer note:
	key is event_id.
	)",

	// typing (key, value)
	{
		typeid(ircd::string_view), typeid(ircd::string_view)
	}
};

const ircd::database::descriptor
ircd::m::dbs::desc::events_prev_events
{
	// name
	"prev_events",

	// explanation
	R"(### protocol note:

	### developer note:
	key is event_id.
	)",

	// typing (key, value)
	{
		typeid(ircd::string_view), typeid(ircd::string_view)
	}
};

const ircd::database::descriptor
ircd::m::dbs::desc::events_prev_state
{
	// name
	"prev_state",

	// explanation
	R"(### protocol note:

	### developer note:
	key is event_id.
	)",

	// typing (key, value)
	{
		typeid(ircd::string_view), typeid(ircd::string_view)
	}
};

/*
const ircd::database::descriptor
event_id_in_sender
{
	// name
	"event_id in sender",

	// explanation
	R"(### developer note:

	key is "@sender$event_id"
	the prefix transform is in effect. this column indexes events by
	sender offering an iterable bound of the index prefixed by sender

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
	event_id_in,
};

/// prefix transform for origin in
///
/// This transform expects a concatenation ending with an origin which means
/// the prefix can be the same for multiple origins; therefor we can find
/// or iterate "origin in X" where X is some repeated prefix
///
/// TODO: strings will have character conflicts. must address
const ircd::db::prefix_transform
origin_in
{
	"origin in",
	[](const ircd::string_view &key)
	{
		return has(key, ":::");
		//return key.find(':') != key.npos;
	},
	[](const ircd::string_view &key)
	{
		return split(key, ":::").first;
		//return rsplit(key, ':').first;
	}
};

const ircd::database::descriptor
origin_in_room_id
{
	// name
	"origin in room_id",

	// explanation
	R"(### developer note:

	key is "!room_id:origin"
	the prefix transform is in effect. this column indexes origins in a
	room_id offering an iterable bound of the index prefixed by room_id

	)",

	// typing (key, value)
	{
		typeid(ircd::string_view), typeid(ircd::string_view)
	},

	// options
	{},

	// comparator - sorts from highest to lowest
	{}, //ircd::db::reverse_cmp_string_view{},

	// prefix transform
	origin_in,
};

const ircd::database::descriptor
origin_joined_in_room_id
{
	// name
	"origin_joined in room_id",

	// explanation
	R"(### developer note:

	key is "!room_id:origin"
	the prefix transform is in effect. this column indexes origins in a
	room_id offering an iterable bound of the index prefixed by room_id

	)",

	// typing (key, value)
	{
		typeid(ircd::string_view), typeid(ircd::string_view)
	},

	// options
	{},

	// comparator - sorts from highest to lowest
	{}, //ircd::db::reverse_cmp_string_view{},

	// prefix transform
	origin_in,
};

/// prefix transform for room_id
///
/// This transform expects a concatenation ending with a room_id which means
/// the prefix can be the same for multiple room_id's; therefor we can find
/// or iterate "room_id in X" where X is some repeated prefix
///
const ircd::db::prefix_transform room_id_in
{
	"room_id in",
	[](const ircd::string_view &key)
	{
		return key.find('!') != key.npos;
	},
	[](const ircd::string_view &key)
	{
		return rsplit(key, '!').first;
	}
};

const ircd::database::descriptor
prev_event_id_for_event_id_in_room_id
{
	// name
	"prev_event_id for event_id in room_id",

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
	event_id_in
};

/// prefix transform for event_id in room_id,type,state_key
///
/// This transform is special for concatenating room_id with type and state_key
/// and event_id in that order with prefix being the room_id,type,state_key. This
/// will index multiple event_ids with the same type,state_key in a room which
/// allows for a temporal depth to the database; event_id for type,state_key only
/// resolves to a single latest event and overwrites itself as per the room state
/// algorithm whereas this can map all of them and then allows for tracing.
///
/// TODO: arbitrary type strings will have character conflicts. must address
/// TODO: with grammars.
const ircd::db::prefix_transform
event_id_in_room_id_type_state_key
{
	"event_id in room_id,type_state_key",
	[](const ircd::string_view &key)
	{
		return has(key, '$');
	},
	[](const ircd::string_view &key)
	{
		return split(key, '$').first;
	}
};

const ircd::database::descriptor
prev_event_id_for_type_state_key_event_id_in_room_id
{
	// name
	"prev_event_id for type,state_key,event_id in room_id",

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
	event_id_in_room_id_type_state_key
};
*/

const ircd::database::description
ircd::m::dbs::desc::events
{
	// Requirement of RocksDB/LevelDB
	{ "default" },

	////////
	//
	// These columns directly represent event fields indexed by event_id and
	// the value is the actual event values. Some values may be JSON, like
	// content.
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

	////////
	//
	// These columns are metadata composed from the event data. Specifically,
	// they are designed for fast sequential iterations.
	//

	// (state tree node id) => (state tree node)
	events__state_node,

	// (room_id, event_id) => (state_root)
	// Sequence of all events for a room, ever.
	events__room_events,

	// (room_id, origin) => ()
	// Sequence of all PRESENTLY JOINED origins for a room.
	events__room_origins,

	// (room_id, type, state_key) => (event_id)
	// Sequence of the PRESENT STATE of the room.
	events__room_state,
};
