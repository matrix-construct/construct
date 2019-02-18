// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_M_DBS_H

/// Database Schema
///
namespace ircd::m::dbs
{
	struct init;
	struct write_opts;

	// General confs
	extern conf::item<bool> events_cache_enable;
	extern conf::item<bool> events_cache_comp_enable;
	extern conf::item<size_t> events_mem_write_buffer_size;
	extern conf::item<size_t> events_sst_write_buffer_size;

	// Database instance
	extern std::shared_ptr<db::database> events;

	// Event property columns
	constexpr const auto event_columns{event::size()};
	extern std::array<db::column, event_columns> event_column;

	// Event metadata columns
	extern db::column event_idx;       // event_id => event_idx
	extern db::column event_json;      // event_idx => full json
	extern db::index event_refs;       // event_idx | ref_type, event_idx
	extern db::index room_head;        // room_id | event_id => event_idx
	extern db::index room_events;      // room_id | depth, event_idx => node_id
	extern db::index room_joined;      // room_id | origin, member => event_idx
	extern db::index room_state;       // room_id | type, state_key => event_idx
	extern db::column state_node;      // node_id => state::node

	// Lowlevel util
	enum class ref :uint8_t;
	constexpr size_t EVENT_REFS_KEY_MAX_SIZE {sizeof(event::idx) + sizeof(event::idx)};
	constexpr size_t ref_shift {8 * (sizeof(event::idx) - sizeof(ref))};
	constexpr event::idx ref_mask {0xFFUL << ref_shift};
	string_view event_refs_key(const mutable_buffer &out, const event::idx &tgt, const ref &type, const event::idx &referer);
	std::tuple<ref, event::idx> event_refs_key(const string_view &amalgam);
	string_view reflect(const ref &);

	constexpr size_t ROOM_HEAD_KEY_MAX_SIZE {id::MAX_SIZE + 1 + id::MAX_SIZE};
	string_view room_head_key(const mutable_buffer &out, const id::room &, const id::event &);
	string_view room_head_key(const string_view &amalgam);

	constexpr size_t ROOM_STATE_KEY_MAX_SIZE {id::MAX_SIZE + event::TYPE_MAX_SIZE + event::STATE_KEY_MAX_SIZE};
	string_view room_state_key(const mutable_buffer &out, const id::room &, const string_view &type, const string_view &state_key);
	string_view room_state_key(const mutable_buffer &out, const id::room &, const string_view &type);
	std::pair<string_view, string_view> room_state_key(const string_view &amalgam);

	constexpr size_t ROOM_JOINED_KEY_MAX_SIZE {id::MAX_SIZE + event::ORIGIN_MAX_SIZE + id::MAX_SIZE};
	string_view room_joined_key(const mutable_buffer &out, const id::room &, const string_view &origin, const id::user &member);
	string_view room_joined_key(const mutable_buffer &out, const id::room &, const string_view &origin);
	std::pair<string_view, string_view> room_joined_key(const string_view &amalgam);

	constexpr size_t ROOM_EVENTS_KEY_MAX_SIZE {id::MAX_SIZE + 1 + 8 + 8};
	string_view room_events_key(const mutable_buffer &out, const id::room &, const uint64_t &depth, const event::idx &);
	string_view room_events_key(const mutable_buffer &out, const id::room &, const uint64_t &depth);
	std::pair<uint64_t, event::idx> room_events_key(const string_view &amalgam);

	// [GET] the state root for an event (with as much information as you have)
	string_view state_root(const mutable_buffer &out, const id::room &, const event::idx &, const uint64_t &depth);
	string_view state_root(const mutable_buffer &out, const id::room &, const event::id &, const uint64_t &depth);
	string_view state_root(const mutable_buffer &out, const id::room &, const event::idx &);
	string_view state_root(const mutable_buffer &out, const id::room &, const event::id &);
	string_view state_root(const mutable_buffer &out, const event::idx &);
	string_view state_root(const mutable_buffer &out, const event::id &);
	string_view state_root(const mutable_buffer &out, const event &);

	// [SET (txn)] Basic write suite
	string_view write(db::txn &, const event &, const write_opts &);
	void blacklist(db::txn &, const event::id &, const write_opts &);
}

/// Options that affect the dbs::write() of an event to the transaction.
struct ircd::m::dbs::write_opts
{
	static const std::bitset<256> event_refs_all;

	/// Operation code
	db::op op {db::op::SET};

	/// Principal's index number. Most codepaths do not permit zero; must set.
	uint64_t event_idx {0};

	/// Whether the event_id should be indexed in event_idx (you want yes).
	bool event_id {true};

	/// Whether the event.source can be used directly for event_json. Defaults
	/// to false unless the caller wants to avoid a redundant re-stringify.
	bool json_source {false};

	/// Selection of what reference types to manipulate in event_refs. Refs
	/// will not be made if it is not appropriate for the event anyway, so
	/// this defaults to all bits. User can disable one or more ref types
	/// by clearing a bit.
	std::bitset<256> event_refs {event_refs_all};

	/// User can supply a view of already-generated keys with event_refs_key().
	/// This vector will be checked first before generating that key, which
	/// can avoid any index() queries internally to generate it.
	vector_view<const string_view> event_refs_hint;

	/// Whether the present state table `room_state` should be updated by
	/// this operation if appropriate.
	bool present {true};

	/// Whether the history state btree `state_node` + `room_events` value
	/// should be updated by this operation if appropriate.
	bool history {true};

	/// The state btree root to perform the update on.
	string_view root_in;

	/// After the update is performed, the new state btree root is returned
	/// into this buffer.
	mutable_buffer root_out;

	/// Whether the event should be added to the room_head, indicating that
	/// it has not yet been referenced at the time of this write. Defaults
	/// to true, but if this is an older event this opt should be rethought.
	bool room_head {true};

	/// Whether the event removes the prev_events it references from the
	/// room_head. This defaults to true and should almost always be true.
	bool room_refs {true};
};

/// Types of references indexed by event_refs. This is a single byte integer,
/// which should be plenty of namespace. Internally event_refs_key() and
/// event_refs store this in a high order byte of an event::idx integer. This
/// is an alternative to having separate columns for each type of reference.
enum class ircd::m::dbs::ref
:uint8_t
{
	// DAG
	PREV                = 0x00,
	AUTH                = 0x01,

	// m.receipt
	M_RECEIPT__M_READ   = 0x10,

	// m.relates_to
	M_RELATES__M_REPLY  = 0x20,
};

/// Database Schema Descriptors
///
namespace ircd::m::dbs::desc
{
	// Full description
	extern const db::description events;

	//
	// Direct columns
	//

	extern conf::item<size_t> events___event__bloom__bits;

	extern conf::item<size_t> events__content__block__size;
	extern conf::item<size_t> events__content__meta_block__size;
	extern conf::item<size_t> events__content__cache__size;
	extern conf::item<size_t> events__content__cache_comp__size;
	extern const db::descriptor events_content;

	extern conf::item<size_t> events__depth__block__size;
	extern conf::item<size_t> events__depth__meta_block__size;
	extern conf::item<size_t> events__depth__cache__size;
	extern conf::item<size_t> events__depth__cache_comp__size;
	extern const db::descriptor events_depth;

	extern conf::item<size_t> events__event_id__block__size;
	extern conf::item<size_t> events__event_id__meta_block__size;
	extern conf::item<size_t> events__event_id__cache__size;
	extern conf::item<size_t> events__event_id__cache_comp__size;
	extern const db::descriptor events_event_id;

	extern conf::item<size_t> events__origin_server_ts__block__size;
	extern conf::item<size_t> events__origin_server_ts__meta_block__size;
	extern conf::item<size_t> events__origin_server_ts__cache__size;
	extern conf::item<size_t> events__origin_server_ts__cache_comp__size;
	extern const db::descriptor events_origin_server_ts;

	extern conf::item<size_t> events__room_id__block__size;
	extern conf::item<size_t> events__room_id__meta_block__size;
	extern conf::item<size_t> events__room_id__cache__size;
	extern conf::item<size_t> events__room_id__cache_comp__size;
	extern const db::descriptor events_room_id;

	extern conf::item<size_t> events__sender__block__size;
	extern conf::item<size_t> events__sender__meta_block__size;
	extern conf::item<size_t> events__sender__cache__size;
	extern conf::item<size_t> events__sender__cache_comp__size;
	extern const db::descriptor events_sender;

	extern conf::item<size_t> events__state_key__block__size;
	extern conf::item<size_t> events__state_key__meta_block__size;
	extern conf::item<size_t> events__state_key__cache__size;
	extern conf::item<size_t> events__state_key__cache_comp__size;
	extern const db::descriptor events_state_key;

	extern conf::item<size_t> events__type__block__size;
	extern conf::item<size_t> events__type__meta_block__size;
	extern conf::item<size_t> events__type__cache__size;
	extern conf::item<size_t> events__type__cache_comp__size;
	extern const db::descriptor events_type;

	//
	// Metadata columns
	//

	// events index
	extern conf::item<size_t> events__event_idx__block__size;
	extern conf::item<size_t> events__event_idx__meta_block__size;
	extern conf::item<size_t> events__event_idx__cache__size;
	extern conf::item<size_t> events__event_idx__cache_comp__size;
	extern conf::item<size_t> events__event_idx__bloom__bits;
	extern const db::descriptor events__event_idx;

	// events cache
	extern conf::item<size_t> events__event_json__block__size;
	extern conf::item<size_t> events__event_json__meta_block__size;
	extern conf::item<size_t> events__event_json__cache__size;
	extern conf::item<size_t> events__event_json__cache_comp__size;
	extern conf::item<size_t> events__event_json__bloom__bits;
	extern const db::descriptor events__event_json;

	// events graphing
	extern conf::item<size_t> events__event_refs__block__size;
	extern conf::item<size_t> events__event_refs__meta_block__size;
	extern conf::item<size_t> events__event_refs__cache__size;
	extern conf::item<size_t> events__event_refs__cache_comp__size;
	extern const db::prefix_transform events__event_refs__pfx;
	extern const db::comparator events__event_refs__cmp;
	extern const db::descriptor events__event_refs;

	// room head mapping sequence
	extern conf::item<size_t> events__room_head__block__size;
	extern conf::item<size_t> events__room_head__meta_block__size;
	extern conf::item<size_t> events__room_head__cache__size;
	extern const db::prefix_transform events__room_head__pfx;
	extern const db::descriptor events__room_head;

	// room events sequence
	extern conf::item<size_t> events__room_events__block__size;
	extern conf::item<size_t> events__room_events__meta_block__size;
	extern conf::item<size_t> events__room_events__cache__size;
	extern conf::item<size_t> events__room_events__cache_comp__size;
	extern const db::prefix_transform events__room_events__pfx;
	extern const db::comparator events__room_events__cmp;
	extern const db::descriptor events__room_events;

	// room present joined members sequence
	extern conf::item<size_t> events__room_joined__block__size;
	extern conf::item<size_t> events__room_joined__meta_block__size;
	extern conf::item<size_t> events__room_joined__cache__size;
	extern conf::item<size_t> events__room_joined__cache_comp__size;
	extern conf::item<size_t> events__room_joined__bloom__bits;
	extern const db::prefix_transform events__room_joined__pfx;
	extern const db::descriptor events__room_joined;

	// room present state mapping sequence
	extern conf::item<size_t> events__room_state__block__size;
	extern conf::item<size_t> events__room_state__meta_block__size;
	extern conf::item<size_t> events__room_state__cache__size;
	extern conf::item<size_t> events__room_state__cache_comp__size;
	extern conf::item<size_t> events__room_state__bloom__bits;
	extern const db::prefix_transform events__room_state__pfx;
	extern const db::descriptor events__room_state;

	// state btree node key-value store
	extern conf::item<size_t> events__state_node__block__size;
	extern conf::item<size_t> events__state_node__meta_block__size;
	extern conf::item<size_t> events__state_node__cache__size;
	extern conf::item<size_t> events__state_node__cache_comp__size;
	extern conf::item<size_t> events__state_node__bloom__bits;
	extern const db::descriptor events__state_node;
}

// Internal interface; not for public.
namespace ircd::m::dbs
{
	void _index__room_state(db::txn &,  const event &, const write_opts &);
	void _index__room_events(db::txn &,  const event &, const write_opts &, const string_view &);
	void _index__room_joined(db::txn &, const event &, const write_opts &);
	void _index__room_head(db::txn &, const event &, const write_opts &);
	string_view _index_state(db::txn &, const event &, const write_opts &);
	string_view _index_redact(db::txn &, const event &, const write_opts &);
	string_view _index_other(db::txn &, const event &, const write_opts &);
	string_view _index_room(db::txn &, const event &, const write_opts &);
	void _index_event_refs_m_receipt_m_read(db::txn &, const event &, const write_opts &);
	void _index_event_refs_m_relates_m_reply(db::txn &, const event &, const write_opts &);
	void _index_event_refs_auth(db::txn &, const event &, const write_opts &);
	void _index_event_refs_prev(db::txn &, const event &, const write_opts &);
	void _index_event_refs(db::txn &, const event &, const write_opts &);
	void _index_event_id(db::txn &, const event &, const write_opts &);
	void _index_event(db::txn &, const event &, const write_opts &);
	void _append_json(db::txn &, const event &, const write_opts &);
	void _append_cols(db::txn &, const event &, const write_opts &);
}

struct ircd::m::dbs::init
{
	init(std::string dbopts = {});
	~init() noexcept;
};
