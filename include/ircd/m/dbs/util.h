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
#define HAVE_IRCD_M_DBS_UTIL_H

namespace ircd::m::dbs
{
	constexpr size_t EVENT_REFS_KEY_MAX_SIZE {sizeof(event::idx) + sizeof(event::idx)};
	constexpr size_t ref_shift {8 * (sizeof(event::idx) - sizeof(ref))};
	constexpr event::idx ref_mask {0xFFUL << ref_shift};
	string_view event_refs_key(const mutable_buffer &out, const event::idx &tgt, const ref &type, const event::idx &referer);
	std::tuple<ref, event::idx> event_refs_key(const string_view &amalgam);
	string_view reflect(const ref &);

	constexpr size_t EVENT_SENDER_KEY_MAX_SIZE {id::MAX_SIZE + 1 + 8};
	string_view event_sender_key(const mutable_buffer &out, const string_view &origin, const string_view &localpart = {}, const event::idx & = 0);
	string_view event_sender_key(const mutable_buffer &out, const id::user &, const event::idx &);
	std::tuple<string_view, event::idx> event_sender_key(const string_view &amalgam);

	constexpr size_t EVENT_TYPE_KEY_MAX_SIZE {event::TYPE_MAX_SIZE + 1 + 8};
	string_view event_type_key(const mutable_buffer &out, const string_view &, const event::idx & = 0);
	std::tuple<event::idx> event_type_key(const string_view &amalgam);

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
}
