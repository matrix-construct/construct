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
#define HAVE_IRCD_M_NAME_H

/// All strings used for json::tuple keys in ircd::m (matrix protocol) are
/// contained within this namespace. These strings allow constant time access
/// to JSON in a tuple using recursive constexpr function inlining provided by
/// ircd::json.
///
/// They're all aggregated here rather than distributing this namespace around
/// to where they're used because:
///
/// * There can't be any duplicates.
/// * Eventually addressing the issue of subobject-linkage might be easier;
///   this namespace could become a struct with linkage if that ever helps.
///
namespace ircd::m::name
{
	constexpr const char *const auth_events {"auth_events"};
	constexpr const char *const content {"content"};
	constexpr const char *const depth {"depth"};
	constexpr const char *const event_id {"event_id"};
	constexpr const char *const hashes {"hashes"};
	constexpr const char *const membership {"membership"};
	constexpr const char *const origin {"origin"};
	constexpr const char *const origin_server_ts {"origin_server_ts"};
	constexpr const char *const prev_events {"prev_events"};
	constexpr const char *const prev_state {"prev_state"};
	constexpr const char *const redacts {"redacts"};
	constexpr const char *const room_id {"room_id"};
	constexpr const char *const sender {"sender"};
	constexpr const char *const signatures {"signatures"};
	constexpr const char *const state_key {"state_key"};
	constexpr const char *const type {"type"};
	constexpr const char *const unsigned_ {"unsigned"};

	constexpr const char *const edus {"edus"};
	constexpr const char *const pdu_failures {"pdu_failures"};
	constexpr const char *const pdus {"pdus"};

	constexpr const char *const destination {"destination"};
	constexpr const char *const method {"method"};
	constexpr const char *const uri {"uri"};

	constexpr const char *const old_verify_keys {"old_verify_keys"};
	constexpr const char *const server_name {"server_name"};
	constexpr const char *const tls_fingerprints {"tls_fingerprints"};
	constexpr const char *const valid_until_ts {"valid_until_ts"};
	constexpr const char *const verify_keys {"verify_keys"};

	constexpr const char *const m_room_aliases {"m.room.aliases"};
	constexpr const char *const m_room_canonical_alias {"m.room.canonical_alias"};
	constexpr const char *const m_room_create {"m.room.create"};
	constexpr const char *const m_room_join_rules {"m.room.join_rules"};
	constexpr const char *const m_room_member {"m.room.member"};
	constexpr const char *const m_room_power_levels {"m.room.power_levels"};
	constexpr const char *const m_room_message {"m.room.message"};
	constexpr const char *const m_room_name {"m.room.name"};
	constexpr const char *const m_room_topic {"m.room.topic"};
	constexpr const char *const m_room_avatar {"m.room.avatar"};
	constexpr const char *const m_room_pinned_events {"m.room.pinned_events"};
	constexpr const char *const m_room_history_visibility {"m.room.history_visibility"};
	constexpr const char *const m_room_third_party_invite {"m.room.third_party_invite"};
	constexpr const char *const m_room_guest_access {"m.room.guest_access"};

	constexpr const char *const event_fields {"event_fields"};
	constexpr const char *const event_format {"event_format"};
	constexpr const char *const account_data {"account_data"};
	constexpr const char *const presence {"presence"};
	constexpr const char *const room {"room"};
	constexpr const char *const timeline {"timeline"};
	constexpr const char *const ephemeral {"ephemeral"};
	constexpr const char *const state {"state"};
	constexpr const char *const rooms {"rooms"};
	constexpr const char *const not_rooms {"not_rooms"};
	constexpr const char *const include_leave {"include_leave"};
	constexpr const char *const types {"types"};
	constexpr const char *const not_types {"not_types"};
	constexpr const char *const senders {"senders"};
	constexpr const char *const not_senders {"not_senders"};
	constexpr const char *const limit {"limit"};

	constexpr const char *const edu_type {"edu_type"};

	constexpr const char *const user_id {"user_id"};
	constexpr const char *const last_active_ago {"last_active_ago"};
	constexpr const char *const currently_active {"currently_active"};
	constexpr const char *const status_msg {"status_msg"};

	constexpr const char *const ts {"ts"};
	constexpr const char *const event_ids {"event_ids"};

	constexpr const char *const typing {"typing"};
}
