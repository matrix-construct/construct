// The Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_M_PUSH_H

namespace ircd::m::push
{
	struct cond;
	struct rule;
	struct rules;
	struct pusher;
	struct match;
	struct request;

	IRCD_M_EXCEPTION(m::error, error, http::INTERNAL_SERVER_ERROR)
	IRCD_M_EXCEPTION(error, NOT_A_RULE, http::BAD_REQUEST)

	/// scope, kind, ruleid
	using path = std::tuple<string_view, string_view, string_view>;
	string_view make_type(const mutable_buffer &, const path &);
	path make_path(const string_view &type, const string_view &state_key);
	path make_path(const event &);

	bool highlighting(const rule &); // true for highlight tweak
	bool notifying(const rule &); // true on "notify" or "coalesce"

	extern log::log log;
}

struct ircd::m::push::request
:instance_list<ircd::m::push::request>
{
	static conf::item<bool> enable;
	static conf::item<seconds> timeout;
	static ctx::mutex mutex;
	static ctx::dock dock;
	static uint64_t id_ctr;

	uint64_t id {++id_ctr};
	event::idx event_idx {0};
	rfc3986::uri url;
	json::object content;
	server::request req;
	http::code code {0};
	json::object response;
	char buf[15_KiB];
};

struct ircd::m::push::match
:boolean
{
	struct opts;
	using cond_kind_func = bool (*)(const event &, const cond &, const opts &);

	static const string_view cond_kind_name[6];
	static const cond_kind_func cond_kind[7];

	explicit match(const event &, const cond &, const opts &);
	explicit match(const event &, const rule &, const opts &);
};

struct ircd::m::push::match::opts
{
	m::id::user user_id;
};

/// 13.13.1 I'm your pusher, baby.
struct ircd::m::push::pusher
:json::tuple
<
	/// Required. This is a unique identifier for this pusher. See /set for
	/// more detail. Max length, 512 bytes.
	json::property<name::pushkey, json::string>,

	/// Required. The kind of pusher. "http" is a pusher that sends HTTP pokes.
	json::property<name::kind, json::string>,

	/// Required. This is a reverse-DNS style identifier for the application.
	/// Max length, 64 chars.
	json::property<name::app_id, json::string>,

	/// Required. A string that will allow the user to identify what
	/// application owns this pusher.
	json::property<name::app_display_name, json::string>,

	/// Required. A string that will allow the user to identify what device owns this pusher.
	json::property<name::device_display_name, json::string>,

	/// This string determines which set of device specific rules this
	/// pusher executes.
	json::property<name::profile_tag, json::string>,

	/// Required. The preferred language for receiving notifications
	/// (e.g. 'en' or 'en-US')
	json::property<name::lang, json::string>,

	/// Required. A dictionary of information for the pusher
	/// implementation itself.
	json::property<name::data, json::object>,

	/// If true, the homeserver should add another pusher with the given
	/// pushkey and App ID in addition to any others with different user IDs.
	/// Otherwise, the homeserver must remove any other pushers with the same
	/// App ID and pushkey for different users. The default is false.
	json::property<name::append, bool>
>
{
	static const string_view type_prefix;

	using super_type::tuple;
	using super_type::operator=;
};

/// 13.13.1.5 Push Ruleset
struct ircd::m::push::rules
:json::tuple
<
	/// These configure behaviour for (unencrypted) messages that match
	/// certain patterns. Content rules take one parameter: pattern, that
	/// gives the glob pattern to match against. This is treated in the same
	/// way as pattern for event_match.
	json::property<name::content, json::array>,

	/// The highest priority rules are user-configured overrides.
	json::property<name::override_, json::array>,

	/// These rules change the behaviour of all messages for a given room. The
	/// rule_id of a room rule is always the ID of the room that it affects.
	json::property<name::room, json::array>,

	/// These rules configure notification behaviour for messages from a
	/// specific Matrix user ID. The rule_id of Sender rules is always the
	/// Matrix user ID of the user whose messages they'd apply to.
	json::property<name::sender, json::array>,

	/// These are identical to override rules, but have a lower priority
	/// than content, room and sender rules.
	json::property<name::underride, json::array>
>
{
	/// Specification pre-defined defaults.
	static const rules defaults;

	using super_type::tuple;
	using super_type::operator=;
};

/// PushRule
struct ircd::m::push::rule
:json::tuple
<
	/// [object or string] Required. The actions to perform when this rule is
	/// matched.
	json::property<name::actions, string_view>,

	/// Required. Whether this is a default rule, or has been set explicitly.
	json::property<name::default_, bool>,

	/// Required. Whether the push rule is enabled or not.
	json::property<name::enabled, bool>,

	/// Required. The ID of this rule.
	json::property<name::rule_id, json::string>,

	/// The conditions that must hold true for an event in order for a rule
	/// to be applied to an event. A rule with no conditions always matches.
	/// Only applicable to underride and override rules.
	json::property<name::conditions, json::array>,

	/// The glob-style pattern to match against. Only applicable to content
	/// rules.
	json::property<name::pattern, json::string>
>
{
	static const string_view type_prefix;

	using closure_bool = std::function<bool (const id::user &, const path &, const json::object &)>;
	static bool for_each(const path &, const closure_bool &);

	using super_type::tuple;
	using super_type::operator=;
};

/// PushCondition
struct ircd::m::push::cond
:json::tuple
<
	/// Required. The kind of condition to apply. See conditions for more
	/// information on the allowed kinds and how they work.
	json::property<name::kind, json::string>,

	/// Required for event_match conditions. The dot- separated field of the
	/// event to match. Required for sender_notification_permission conditions.
	/// The field in the power level event the user needs a minimum power level
	/// for. Fields must be specified under the notifications property in the
	/// power level event's content.
	json::property<name::key, json::string>,

	/// Required for event_match conditions. The glob-style pattern to match
	/// against. Patterns with no special glob characters should be treated
	/// as having asterisks prepended and appended when testing the condition.
	json::property<name::pattern, json::string>,

	/// Required for room_member_count conditions. A decimal integer optionally
	/// prefixed by one of, ==, <, >, >= or <=. A prefix of < matches rooms
	/// where the member count is strictly less than the given number and so
	/// forth. If no prefix is present, this parameter defaults to ==.
	json::property<name::is, json::string>
>
{
	using super_type::tuple;
	using super_type::operator=;
};
