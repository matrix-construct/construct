// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_M_DBS_DBS_H

/// Database Schema
namespace ircd::m::dbs
{
	struct init;
	struct write_opts;
	enum class ref :uint8_t;

	// General confs
	extern conf::item<bool> events_cache_enable;
	extern conf::item<bool> events_cache_comp_enable;
	extern conf::item<size_t> events_mem_write_buffer_size;
	extern conf::item<size_t> events_sst_write_buffer_size;

	// Database instance
	extern std::shared_ptr<db::database> events;

	// [SET (txn)] Basic write suite
	void write(db::txn &, const event &, const write_opts &);
}

/// Database description
namespace ircd::m::dbs::desc
{
	extern const db::description events;
}

/// Transaction appendage builder namespace.
namespace ircd::m::dbs::appendix
{
	enum index :uint8_t;
}

#include "event_column.h"           // event_idx => (direct value)
#include "event_idx.h"              // event_id => event_idx
#include "event_json.h"             // event_idx => (full JSON)
#include "event_refs.h"             // eventidx | ref_type, event_idx
#include "event_horizon.h"          // event_id | event_idx
#include "event_type.h"             // type | event_idx
#include "event_sender.h"           // hostpart | localpart, event_idx
#include "room_head.h"              // room_id | event_id => event_idx
#include "room_events.h"            // room_id | depth, event_idx
#include "room_joined.h"            // room_id | origin, member => event_idx
#include "room_state.h"             // room_id | type, state_key => event_idx
#include "room_state_space.h"       // room_id | type, state_key, depth, event_idx

/// Options that affect the dbs::write() of an event to the transaction.
struct ircd::m::dbs::write_opts
{
	static const std::bitset<256> event_refs_all;
	static const std::bitset<64> appendix_all;

	/// Operation code; usually SET or DELETE. Note that we interpret the
	/// code internally and may set different codes for appendages of the
	/// actual transaction.
	db::op op {db::op::SET};

	/// Principal's index number. Most codepaths do not permit zero. This may
	/// be zero for blacklisting, but the blacklist option must be set.
	uint64_t event_idx {0};

	/// Fuse panel to toggle transaction elements.
	std::bitset<64> appendix {appendix_all};

	/// Selection of what reference types to manipulate in event_refs. Refs
	/// will not be made if it is not appropriate for the event anyway, so
	/// this defaults to all bits. User can disable one or more ref types
	/// by clearing a bit.
	std::bitset<256> event_refs {event_refs_all};

	/// Selection of what reference types to resolve and delete from the
	/// event_horizon for this event.
	std::bitset<256> horizon_resolve {event_refs_all};

	/// Whether the event.source can be used directly for event_json. Defaults
	/// to false unless the caller wants to avoid a redundant re-stringify.
	bool json_source {false};

	/// Data in this db::txn is used as a primary source in some cases where
	/// indexers make a database query. This is useful when the sought data
	/// has not even been written to the database, and this may even point to
	/// the same db::txn as the result being composed in the first place. By
	/// default a database query is made as a fallback after using this.
	const db::txn *interpose {nullptr};

	/// Whether indexers are allowed to make database queries when composing
	/// the transaction. note: database queries may yield the ircd::ctx and
	/// made indepdently; this is slow and requires external synchronization
	/// to not introduce inconsistent data into the txn.
	bool allow_queries {true};

	/// Setting to true allows the event_idx to be 0 which allows the insertion
	/// of the event_id into a "blacklist" to mark it as unprocessable; this
	/// prevents the server from repeatedly trying to process an event.
	///
	/// Note for now this just creates an entry in _event_idx of 0 for the
	/// event_id which also means "not found" for most codepaths, a reasonable
	/// default. But for codepaths that must distinguish between "not found"
	/// and "blacklist" they must know that `event_id => 0` was *found* to be
	/// zero.
	bool blacklist {false};
};

/// Values which represent some element(s) included in a transaction or
/// codepaths taken to construct a transaction. This enum is generally used
/// in a bitset in dbs::write_opts to control the behavior of dbs::write().
///
enum ircd::m::dbs::appendix::index
:std::underlying_type<ircd::m::dbs::appendix::index>::type
{
	/// Involves the event_idx column; translates an event_id to our internal
	/// index number. This bit can be dark during re-indexing operations.
	EVENT_ID,

	/// Involves the event_json column; writes a full JSON serialization
	/// of the event. See the `json_source` write_opts option. This bit can be
	/// dark during re-indexing operations to avoid rewriting the same data.
	EVENT_JSON,

	/// Involves any direct event columns; such columns are forward-indexed
	/// values from the original event data but split into columns for each
	/// property. Can be dark during re-indexing similar to EVENT_JSON.
	EVENT_COLS,

	/// Take branch to handle event reference graphing. A separate bitset is
	/// offered in write_opts for fine-grained control over which reference
	/// types are involved.
	EVENT_REFS,

	/// Involves the event_horizon column which saves the event_id of any
	/// unresolved event_refs at the time of the transaction. This is important
	/// for out-of-order writes to the database. When the unresolved prev_event
	/// is encountered later and finds its event_id in event_horizon it can
	/// properly complete the event_refs graph to all the referencing events.
	EVENT_HORIZON,

	/// Resolves unresolved references for this event left in event_horizon.
	EVENT_HORIZON_RESOLVE,

	/// Involves the event_sender column (reverse index on the event sender).
	EVENT_SENDER,

	/// Involves the event_type column (reverse index on the event type).
	EVENT_TYPE,

	/// Involves room_events table.
	ROOM_EVENTS,

	/// Whether the event should be added to the room_head, indicating that
	/// it has not yet been referenced at the time of this write. Defaults
	/// to true, but if this is an older event this opt should be rethought.
	ROOM_HEAD,

	/// Whether the event removes the prev_events it references from the
	/// room_head. This defaults to true and should almost always be true.
	ROOM_HEAD_RESOLVE,

	/// Involves room_state (present state) table.
	ROOM_STATE,

	/// Involves room_space (all states) table.
	ROOM_STATE_SPACE,

	/// Involves room_joined table.
	ROOM_JOINED,

	/// Take branch to handle room redaction events.
	ROOM_REDACT,
};

struct ircd::m::dbs::init
{
	std::string our_dbpath;
	string_view their_dbpath;

  public:
	init(const string_view &servername, std::string dbopts = {});
	~init() noexcept;
};
