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
#define HAVE_IRCD_M_ROOM_POWER_H

/// Interface to the power levels
///
/// This interface focuses specifically on making the power levels accessible
/// to developers for common query and manipulation operations. power_levels
/// is a single state event in the room containing integer thresholds for
/// privileges in the room in addition to a list of users mapping to an integer
/// for their level. This interface hides the details of that event by
/// presenting single operations which can appear succinctly in IRCd code.
///
struct ircd::m::room::power
{
	using closure = std::function<void (const string_view &, const int64_t &)>;
	using closure_bool = std::function<bool (const string_view &, const int64_t &)>;

	static const int64_t default_creator_level;
	static const int64_t default_power_level;
	static const int64_t default_event_level;
	static const int64_t default_user_level;

	m::room room;
	event::idx power_event_idx {0};
	json::object power_event_content;
	m::id::user room_creator_id;

	bool view(const std::function<void (const json::object &)> &) const;

  public:
	// Iterate a collection usually either "events" or "users" as per spec.
	bool for_each(const string_view &prop, const closure_bool &) const;
	void for_each(const string_view &prop, const closure &) const;

	// Iterates all of the integer levels, excludes the collections.
	bool for_each(const closure_bool &) const;
	void for_each(const closure &) const;

	bool has_level(const string_view &prop) const;
	bool has_collection(const string_view &prop) const;
	bool has_event(const string_view &type) const;
	bool has_user(const m::id::user &) const;

	size_t count(const string_view &prop) const;
	size_t count_collections() const;
	size_t count_levels() const;

	// This suite queries with full defaulting logic as per the spec. These
	// always return suitable results. When determining power to change a state
	// event rather than a non-state event, the state_key must always be
	// defined. If the state_key is a default constructed string_view{} (which
	// means !defined(state_key) and is not the same as string_view{""} for
	// the common state_key="") then the interface considers your query for
	// a non-state event rather than a state_event. Be careful.
	int64_t level(const string_view &prop) const;
	int64_t level_event(const string_view &type, const string_view &state_key) const;
	int64_t level_event(const string_view &type) const;
	int64_t level_user(const m::id::user &) const;

	// all who attain great power and riches make use of either force or fraud"
	bool operator()(const m::id::user &, const string_view &prop, const string_view &type = {}, const string_view &state_key = {}) const;

	explicit power(const json::object &power_event_content, const m::id::user &room_creator_id);
	explicit power(const m::event &power_event, const m::id::user &room_creator_id);
	explicit power(const m::event &power_event, const m::event &create_event);
	power(const m::room &, const event::idx &power_event_idx);
	power(const m::room &);
	power() = default;

	static json::object default_content(const mutable_buffer &out, const m::id::user &creator);
};
