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
#define HAVE_IRCD_M_ROOM_STATE_H

/// Interface to room state.
///
/// This interface focuses specifically on the details of room state. Most of
/// the queries to this interface respond in logarithmic time. If an event with
/// a state_key is present in room::messages but it is not present in
/// room::state (state tree) it was accepted into the room but we will not
/// apply it to our machine, though other parties may (this is a
/// state-conflict).
///
struct ircd::m::room::state
{
	struct opts;

	using keys = std::function<void (const string_view &)>;
	using keys_bool = std::function<bool (const string_view &)>;
	using types = std::function<void (const string_view &)>;
	using types_bool = std::function<bool (const string_view &)>;

	static conf::item<bool> disable_history;
	static conf::item<size_t> readahead_size;

	room::id room_id;
	event::id::buf event_id;
	m::state::id_buffer root_id_buf;
	m::state::id root_id;
	const event::fetch::opts *fopts {nullptr};
	mutable bool _not_present {false}; // cached result of !present()

	bool present() const;

	bool for_each(const types_bool &) const;
	void for_each(const types &) const;
	bool for_each(const string_view &type, const keys_bool &view) const;
	void for_each(const string_view &type, const keys &) const;
	bool for_each(const string_view &type, const string_view &lower_bound, const keys_bool &view) const;
	bool for_each(const string_view &type, const string_view &lower_bound, const event::closure_idx_bool &view) const;
	bool for_each(const string_view &type, const string_view &lower_bound, const event::id::closure_bool &view) const;
	bool for_each(const string_view &type, const string_view &lower_bound, const event::closure_bool &view) const;
	bool for_each(const string_view &type, const event::closure_idx_bool &view) const;
	void for_each(const string_view &type, const event::closure_idx &) const;
	bool for_each(const string_view &type, const event::id::closure_bool &view) const;
	void for_each(const string_view &type, const event::id::closure &) const;
	bool for_each(const string_view &type, const event::closure_bool &view) const;
	void for_each(const string_view &type, const event::closure &) const;
	bool for_each(const event::closure_idx_bool &view) const;
	void for_each(const event::closure_idx &) const;
	bool for_each(const event::id::closure_bool &view) const;
	void for_each(const event::id::closure &) const;
	bool for_each(const event::closure_bool &view) const;
	void for_each(const event::closure &) const;

	// Counting / Statistics
	size_t count(const string_view &type) const;
	size_t count() const;

	// Existential
	bool has(const string_view &type, const string_view &state_key) const;
	bool has(const string_view &type) const;

	// Fetch a state event
	bool get(std::nothrow_t, const string_view &type, const string_view &state_key, const event::closure_idx &) const;
	bool get(std::nothrow_t, const string_view &type, const string_view &state_key, const event::id::closure &) const;
	bool get(std::nothrow_t, const string_view &type, const string_view &state_key, const event::closure &) const;
	void get(const string_view &type, const string_view &state_key, const event::closure_idx &) const;
	void get(const string_view &type, const string_view &state_key, const event::id::closure &) const;
	void get(const string_view &type, const string_view &state_key, const event::closure &) const;

	// Fetch and return state event id
	event::id::buf get(std::nothrow_t, const string_view &type, const string_view &state_key = "") const;
	event::id::buf get(const string_view &type, const string_view &state_key = "") const;

	// Initiate a database prefetch on the state to cache for future access.
	size_t prefetch(const string_view &type, const event::idx &start = 0, const event::idx &stop = 0) const;
	size_t prefetch(const event::idx &start = 0, const event::idx &stop = 0) const;

	state(const m::room &room, const event::fetch::opts *const & = nullptr);
	state() = default;
	state(const state &) = delete;
	state &operator=(const state &) = delete;
};
