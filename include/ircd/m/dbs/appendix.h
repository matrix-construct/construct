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
#define HAVE_IRCD_M_DBS_APPENDIX_H

/// This namespace contains the details and implementation for constructing
/// database transactions through dbs::write().
namespace ircd::m::dbs::appendix
{
	enum index :uint8_t;
}

/// Values which represent some element(s) included in a transaction or
/// codepaths taken to construct a transaction. This enum is generally used
/// in a bitset in dbs::write_opts to control the behavior of dbs::write().
///
enum ircd::m::dbs::appendix::index
:std::underlying_type<ircd::m::dbs::appendix::index>::type
{
	/// This bit offers coarse control over all the EVENT_ appendices.
	EVENT,

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

	/// Involves the event_sender column (reverse index on the event sender).
	EVENT_SENDER,

	/// Involves the event_type column (reverse index on the event type).
	EVENT_TYPE,

	/// Take branch to handle events with a room_id
	ROOM,

	/// Take branch to handle room state events.
	STATE,

	/// Perform state btree manip for room history.
	HISTORY,

	/// Take branch to handle room redaction events.
	REDACT,

	/// Take branch to handle other types of events.
	OTHER,

	/// Whether the event should be added to the room_head, indicating that
	/// it has not yet been referenced at the time of this write. Defaults
	/// to true, but if this is an older event this opt should be rethought.
	ROOM_HEAD,

	/// Whether the event removes the prev_events it references from the
	/// room_head. This defaults to true and should almost always be true.
	ROOM_HEAD_REFS,

	/// Involves room_events table.
	ROOM_EVENTS,

	/// Involves room_joined table.
	ROOM_JOINED,

	/// Involves room_state (present state) table.
	ROOM_STATE,
};

// Internal interface; not for public. (TODO: renamespace)
namespace ircd::m::dbs
{
	void _index__room_state(db::txn &,  const event &, const write_opts &);
	void _index__room_events(db::txn &,  const event &, const write_opts &, const string_view &);
	void _index__room_joined(db::txn &, const event &, const write_opts &);
	void _index__room_head_refs(db::txn &, const event &, const write_opts &);
	void _index__room_head(db::txn &, const event &, const write_opts &);
	string_view _index_state(db::txn &, const event &, const write_opts &);
	string_view _index_redact(db::txn &, const event &, const write_opts &);
	string_view _index_other(db::txn &, const event &, const write_opts &);
	string_view _index_room(db::txn &, const event &, const write_opts &);
	void _index_event_type(db::txn &, const event &, const write_opts &);
	void _index_event_sender(db::txn &, const event &, const write_opts &);
	void _index_event_horizon(db::txn &, const event &, const write_opts &, const id::event &);
	void _index_event_refs_m_room_redaction(db::txn &, const event &, const write_opts &);
	void _index_event_refs_m_receipt_m_read(db::txn &, const event &, const write_opts &);
	void _index_event_refs_m_relates_m_reply(db::txn &, const event &, const write_opts &);
	void _index_event_refs_state(db::txn &, const event &, const write_opts &);
	void _index_event_refs_auth(db::txn &, const event &, const write_opts &);
	void _index_event_refs_prev(db::txn &, const event &, const write_opts &);
	void _index_event_refs(db::txn &, const event &, const write_opts &);
	void _index_event_json(db::txn &, const event &, const write_opts &);
	void _index_event_cols(db::txn &, const event &, const write_opts &);
	void _index_event_id(db::txn &, const event &, const write_opts &);
	void _index_event(db::txn &, const event &, const write_opts &);
}
