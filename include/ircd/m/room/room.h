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
#define HAVE_IRCD_M_ROOM_ROOM_H

/// Interface to a room.
///
/// This is a lightweight object which uses a room_id and an optional event_id
/// to provide an interface to a matrix room. This object itself isn't the
/// actual room data since that takes the form of events in the database;
/// this is just a handle with aforementioned string_view's used by its member
/// functions.
///
/// This object allows the programmer to represent the room either at its
/// present state, or if an event_id is given, at the point of that event.
///
/// Many convenience functions are provided outside of this class.
/// Additionally, several sub-classes provide functionality even more specific
/// than this interface too. If a subclass is provided, for example:
/// `struct members`, such an interface may employ optimized tactics for its
/// specific task.
///
struct ircd::m::room
{
	struct messages;
	struct state;
	struct members;
	struct origins;
	struct head;
	struct auth;
	struct power;
	struct aliases;
	struct stats;

	using id = m::id::room;
	using alias = m::id::room_alias;
	using closure = std::function<void (const room &)>;
	using closure_bool = std::function<bool (const room &)>;

	id room_id;
	event::id event_id;
	const vm::copts *copts {nullptr};
	const event::fetch::opts *fopts {nullptr};

	operator const id &() const;

	// Convenience passthru to room::messages (linear query; newest first)
	bool for_each(const string_view &type, const event::closure_idx_bool &) const;
	void for_each(const string_view &type, const event::closure_idx &) const;
	bool for_each(const string_view &type, const event::id::closure_bool &) const;
	void for_each(const string_view &type, const event::id::closure &) const;
	bool for_each(const string_view &type, const event::closure_bool &) const;
	void for_each(const string_view &type, const event::closure &) const;
	bool for_each(const event::closure_idx_bool &) const;
	void for_each(const event::closure_idx &) const;
	bool for_each(const event::id::closure_bool &) const;
	void for_each(const event::id::closure &) const;
	bool for_each(const event::closure_bool &) const;
	void for_each(const event::closure &) const;

	// Convenience passthru to room::state (logarithmic query)
	bool has(const string_view &type, const string_view &state_key) const;
	bool get(std::nothrow_t, const string_view &type, const string_view &state_key, const event::closure &) const;
	void get(const string_view &type, const string_view &state_key, const event::closure &) const;
	event::idx get(std::nothrow_t, const string_view &type, const string_view &state_key) const;
	event::idx get(const string_view &type, const string_view &state_key) const;

	// Convenience passthru to room::messages (linear query)
	bool has(const string_view &type) const;
	bool get(std::nothrow_t, const string_view &type, const event::closure &) const;
	void get(const string_view &type, const event::closure &) const;

	// misc / convenience utils
	bool membership(const m::id::user &, const string_view &membership = "join") const;
	string_view membership(const mutable_buffer &out, const m::id::user &) const;
	bool visible(const string_view &mxid, const m::event *const & = nullptr) const;
	string_view join_rule(const mutable_buffer &out) const;
	id::user::buf any_user(const string_view &host, const string_view &membership = "join") const;
	bool join_rule(const string_view &rule) const;
	bool lonly() const;

	room(const id &room_id,
	     const string_view &event_id,
	     const vm::copts *const &copts = nullptr,
	     const event::fetch::opts *const &fopts = nullptr)
	:room_id{room_id}
	,event_id{event_id? event::id{event_id} : event::id{}}
	,copts{copts}
	,fopts{fopts}
	{}

	room(const id &room_id,
	     const vm::copts *const &copts = nullptr,
	     const event::fetch::opts *const &fopts = nullptr)
	:room_id{room_id}
	,copts{copts}
	,fopts{fopts}
	{}

	room() = default;

	// Index of create event
	static event::idx index(const id &, std::nothrow_t);
	static event::idx index(const id &);

	static size_t purge(const room &); // cuidado!
};

inline ircd::m::room::operator
const ircd::m::room::id &()
const
{
	return room_id;
}
