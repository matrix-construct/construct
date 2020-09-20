// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

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
	extern const ircd::db::descriptor events__state_node;

	//
	// Required by RocksDB
	//

	extern const ircd::db::descriptor events__default;
};

const ircd::db::prefix_transform
ircd::m::dbs::desc::events__event_auth__pfx
{
	"_event_auth",
	nullptr,
	nullptr,
};

const ircd::db::comparator
ircd::m::dbs::desc::events__event_auth__cmp
{
	"_event_auth",
	nullptr,
	nullptr,
};

const ircd::db::descriptor
ircd::m::dbs::desc::events__event_auth
{
	// name
	"_event_auth",

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
ircd::m::dbs::desc::events__state_node
{
	// name
	"_state_node",

	// explanation
	R"(

	This column is deprecated and has been dropped from the schema. This
	descriptor will erase its presence in the database upon next open.

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
	true,
};

const ircd::db::descriptor
ircd::m::dbs::desc::events__default
{
	// name
	"default",

	// explanation
	R"(Unused but required by the database software.

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

decltype(ircd::m::dbs::desc::events)
ircd::m::dbs::desc::events
{
	// Requirement of RocksDB/LevelDB
	events__default,

	//
	// These columns directly represent event fields indexed by event_idx
	// number and the value is the actual event values. Some values may be
	// JSON, like content.
	//

	content,
	depth,
	event_id,
	origin_server_ts,
	room_id,
	sender,
	state_key,
	type,

	//
	// These columns are metadata oriented around the event data.
	//

	// event_id => uint64_t
	// Mapping of event_id to index number.
	event_idx,

	// event_idx => json
	// Mapping of event_idx to full json
	event_json,

	// event_idx | event_idx
	// Reverse mapping of the event reference graph.
	event_refs,

	// event_idx | event_idx
	// Mapping of unresolved event refs.
	event_horizon,

	// origin | sender, event_idx
	// Mapping of senders to event_idx's they are the sender of.
	event_sender,

	// type | event_idx
	// Mapping of type strings to event_idx's of that type.
	event_type,

	// state_key, type, room_id, depth, event_idx
	// Mapping of event states, indexed for application features.
	event_state,

	// (room_id, (depth, event_idx))
	// Sequence of all events for a room, ever.
	room_events,

	// (room_id, (type, depth, event_idx))
	// Sequence of all events by type for a room.
	room_type,

	// (room_id, (origin, user_id))
	// Sequence of all PRESENTLY JOINED joined for a room.
	room_joined,

	// (room_id, (type, state_key)) => (event_idx)
	// Sequence of the PRESENT STATE of the room.
	room_state,

	// (room_id, (type, state_key, depth, event_idx))
	// Sequence of all states of the room.
	room_state_space,

	// (room_id, event_id) => (event_idx)
	// Mapping of all current head events for a room.
	room_head,

	//
	// These columns are legacy; they have been dropped from the schema.
	//

	events_auth_events,
	events_hashes,
	events_membership,
	events_origin,
	events_prev_events,
	events_prev_state,
	events_redacts,
	events_signatures,
	events__event_auth,
	events__event_bad,
	events__state_node,
};
