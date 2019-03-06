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

namespace ircd::m
{
	struct name;
}

/// All strings used for json::tuple keys in ircd::m (matrix protocol) are
/// contained within this namespace. These strings allow constant time access
/// to JSON in a tuple using recursive constexpr function inlining provided by
/// ircd::json.
///
/// They're all aggregated here rather than distributing this namespace around
/// to where they're used because:
///
/// * There can't be any duplicates.
/// * Addressing the issue of subobject-linkage.
///
struct ircd::m::name
{
	static constexpr const char *const auth_events {"auth_events"};
	static constexpr const char *const content {"content"};
	static constexpr const char *const depth {"depth"};
	static constexpr const char *const event_id {"event_id"};
	static constexpr const char *const hashes {"hashes"};
	static constexpr const char *const membership {"membership"};
	static constexpr const char *const origin {"origin"};
	static constexpr const char *const origin_server_ts {"origin_server_ts"};
	static constexpr const char *const prev_events {"prev_events"};
	static constexpr const char *const prev_state {"prev_state"};
	static constexpr const char *const redacts {"redacts"};
	static constexpr const char *const room_id {"room_id"};
	static constexpr const char *const sender {"sender"};
	static constexpr const char *const signatures {"signatures"};
	static constexpr const char *const state_key {"state_key"};
	static constexpr const char *const type {"type"};
	static constexpr const char *const unsigned_ {"unsigned"};

	static constexpr const char *const edus {"edus"};
	static constexpr const char *const pdu_failures {"pdu_failures"};
	static constexpr const char *const pdus {"pdus"};

	static constexpr const char *const destination {"destination"};
	static constexpr const char *const method {"method"};
	static constexpr const char *const uri {"uri"};

	static constexpr const char *const old_verify_keys {"old_verify_keys"};
	static constexpr const char *const server_name {"server_name"};
	static constexpr const char *const tls_fingerprints {"tls_fingerprints"};
	static constexpr const char *const valid_until_ts {"valid_until_ts"};
	static constexpr const char *const verify_keys {"verify_keys"};

	static constexpr const char *const m_room_aliases {"m.room.aliases"};
	static constexpr const char *const m_room_canonical_alias {"m.room.canonical_alias"};
	static constexpr const char *const m_room_create {"m.room.create"};
	static constexpr const char *const m_room_join_rules {"m.room.join_rules"};
	static constexpr const char *const m_room_member {"m.room.member"};
	static constexpr const char *const m_room_power_levels {"m.room.power_levels"};
	static constexpr const char *const m_room_message {"m.room.message"};
	static constexpr const char *const m_room_name {"m.room.name"};
	static constexpr const char *const m_room_topic {"m.room.topic"};
	static constexpr const char *const m_room_avatar {"m.room.avatar"};
	static constexpr const char *const m_room_pinned_events {"m.room.pinned_events"};
	static constexpr const char *const m_room_history_visibility {"m.room.history_visibility"};
	static constexpr const char *const m_room_third_party_invite {"m.room.third_party_invite"};
	static constexpr const char *const m_room_guest_access {"m.room.guest_access"};

	static constexpr const char *const event_fields {"event_fields"};
	static constexpr const char *const event_format {"event_format"};
	static constexpr const char *const account_data {"account_data"};
	static constexpr const char *const presence {"presence"};
	static constexpr const char *const room {"room"};
	static constexpr const char *const timeline {"timeline"};
	static constexpr const char *const ephemeral {"ephemeral"};
	static constexpr const char *const state {"state"};
	static constexpr const char *const rooms {"rooms"};
	static constexpr const char *const not_rooms {"not_rooms"};
	static constexpr const char *const include_leave {"include_leave"};
	static constexpr const char *const types {"types"};
	static constexpr const char *const not_types {"not_types"};
	static constexpr const char *const senders {"senders"};
	static constexpr const char *const not_senders {"not_senders"};
	static constexpr const char *const limit {"limit"};
	static constexpr const char *const contains_url {"contains_url"};

	static constexpr const char *const edu_type {"edu_type"};

	static constexpr const char *const user_id {"user_id"};
	static constexpr const char *const last_active_ago {"last_active_ago"};
	static constexpr const char *const currently_active {"currently_active"};
	static constexpr const char *const status_msg {"status_msg"};

	static constexpr const char *const ts {"ts"};
	static constexpr const char *const data {"data"};
	static constexpr const char *const event_ids {"event_ids"};

	static constexpr const char *const typing {"typing"};
	static constexpr const char *const timeout {"timeout"};

	static constexpr const char *const user {"user"};
	static constexpr const char *const medium {"medium"};
	static constexpr const char *const address {"address"};
	static constexpr const char *const password {"password"};
	static constexpr const char *const token {"token"};
	static constexpr const char *const device_id {"device_id"};
	static constexpr const char *const initial_device_display_name {"initial_device_display_name"};
	static constexpr const char *const identifier {"identifier"};

	static constexpr const char *const username {"username"};
	static constexpr const char *const bind_email {"bind_email"};
	static constexpr const char *const auth {"auth"};
	static constexpr const char *const inhibit_login {"inhibit_login"};

	static constexpr const char *const visibility {"visibility"};
	static constexpr const char *const room_alias_name {"room_alias_name"};
	static constexpr const char *const name_ {"name"};
	static constexpr const char *const topic {"topic"};
	static constexpr const char *const invite {"invite"};
	static constexpr const char *const invite_3pid {"invite_3pid"};
	static constexpr const char *const creation_content {"creation_content"};
	static constexpr const char *const initial_state {"initial_state"};
	static constexpr const char *const preset {"preset"};
	static constexpr const char *const is_direct {"is_direct"};
	static constexpr const char *const guest_can_join {"guest_can_join"};
	static constexpr const char *const power_level_content_override {"power_level_content_override"};
	static constexpr const char *const parent_room_id {"parent_room_id"};
	static constexpr const char *const creator {"creator"};

	static constexpr const char *const id_server {"id_server"};

	static constexpr const char *const id {"id"};
	static constexpr const char *const url {"url"};
	static constexpr const char *const as_token {"as_token"};
	static constexpr const char *const hs_token {"hs_token"};
	static constexpr const char *const sender_localpart {"sender_localpart"};
	static constexpr const char *const namespaces {"namespaces"};
	static constexpr const char *const rate_limited {"rate_limited"};
	static constexpr const char *const protocols {"protocols"};

	static constexpr const char *const users {"users"};
	static constexpr const char *const aliases {"aliases"};

	static constexpr const char *const exclusive {"exclusive"};
	static constexpr const char *const regex {"regex"};

	static constexpr const char *const display_name {"display_name"};
	static constexpr const char *const last_seen_ip {"last_seen_ip"};
	static constexpr const char *const last_seen_ts {"last_seen_ts"};
	static constexpr const char *const keys {"keys"};
	static constexpr const char *const device_display_name {"device_display_name"};
	static constexpr const char *const algorithms {"algorithms"};

	static constexpr const char *const message_id {"message_id"};
	static constexpr const char *const messages {"messages"};

	static constexpr const char *const stream_id {"stream_id"};
	static constexpr const char *const prev_id {"prev_id"};
	static constexpr const char *const deleted {"deleted"};
	static constexpr const char *const access_token_id {"access_token_id"};
};
