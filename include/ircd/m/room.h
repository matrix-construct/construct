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
#define HAVE_IRCD_M_ROOM_H
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsubobject-linkage"

namespace ircd::m
{
	IRCD_M_EXCEPTION(m::error, CONFLICT, http::CONFLICT);
	IRCD_M_EXCEPTION(m::error, NOT_MODIFIED, http::NOT_MODIFIED);
	IRCD_M_EXCEPTION(CONFLICT, ALREADY_MEMBER, http::CONFLICT);

	struct room;

	bool my(const room &);
	bool exists(const id::room &);

	// [GET] Current Event suite (non-locking) (one)
	id::event::buf head(const id::room &, int64_t &);  // gives depth
	id::event::buf head(const id::room &);
	int64_t depth(const id::room &);

	// [SET] Lowest-level
	event::id::buf commit(const room &, json::iov &event, const json::iov &content);

	// [SET] Send state to room
	event::id::buf send(const room &, const m::id::user &sender, const string_view &type, const string_view &state_key, const json::iov &content);
	event::id::buf send(const room &, const m::id::user &sender, const string_view &type, const string_view &state_key, const json::members &content);
	event::id::buf send(const room &, const m::id::user &sender, const string_view &type, const string_view &state_key, const json::object &content);

	// [SET] Send non-state to room
	event::id::buf send(const room &, const m::id::user &sender, const string_view &type, const json::iov &content);
	event::id::buf send(const room &, const m::id::user &sender, const string_view &type, const json::members &content);
	event::id::buf send(const room &, const m::id::user &sender, const string_view &type, const json::object &content);

	// [SET] Convenience sends
	event::id::buf message(const room &, const m::id::user &sender, const json::members &content);
	event::id::buf message(const room &, const m::id::user &sender, const string_view &body, const string_view &msgtype = "m.text");
	event::id::buf membership(const room &, const m::id::user &, const string_view &membership);
	event::id::buf leave(const room &, const m::id::user &);
	event::id::buf join(const room &, const m::id::user &);

	// [SET] Create new room
	room create(const id::room &, const id::user &creator, const id::room &parent, const string_view &type);
	room create(const id::room &, const id::user &creator, const string_view &type = {});
}

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
/// Many convenience functions are provided outside of this class to
/// accomplish general tasks using rooms in one statement. Additionally,
/// several sub-classes provide functionality even more specific than this
/// interface too. If a subclass is provided, for example: `struct members`,
/// such an interface may employ optimized tactics for its specific task.
///
struct ircd::m::room
{
	struct state;
	struct members;
	struct messages;
	struct origins;

	using id = m::id::room;
	using alias = m::id::room_alias;

	id room_id;
	event::id event_id;

	operator const id &() const        { return room_id;                       }

	// X
	bool get(const event::closure &) const;
	void for_each(const event::closure &) const;

	// misc
	bool membership(const m::id::user &, const string_view &membership = "join") const;

	room(const alias &, const event::id &event_id = {});
	room(const id &room_id, const event::id &event_id = {})
	:room_id{room_id}
	,event_id{event_id}
	{}
};

/// Interface to room state.
///
/// This interface focuses specifically on the details of room state.
///
struct ircd::m::room::state
{
	struct tuple;

	m::room room;

	// Get the state root node ID
	string_view root(m::state::id_buffer &) const;

	// Iterate the state; for_each protocol
	void for_each(const string_view &type, const event::id::closure &) const;
	void for_each(const string_view &type, const event::closure &) const;
	void for_each(const event::id::closure &) const;
	void for_each(const event::closure &) const;

	// Iterate the state; test protocol
	bool test(const string_view &type, const event::id::closure_bool &view) const;
	bool test(const string_view &type, const event::closure_bool &view) const;
	bool test(const event::id::closure_bool &view) const;
	bool test(const event::closure_bool &view) const;

	// Existential
	bool has(const string_view &type, const string_view &state_key) const;
	bool has(const string_view &type) const;

	// Fetch a state event
	bool get(std::nothrow_t, const string_view &type, const string_view &state_key, const event::id::closure &) const;
	bool get(std::nothrow_t, const string_view &type, const string_view &state_key, const event::closure &) const;
	void get(const string_view &type, const string_view &state_key, const event::id::closure &) const;
	void get(const string_view &type, const string_view &state_key, const event::closure &) const;

	state(m::room room)
	:room{room}
	{}
};

/// Interface to the members of a room.
///
/// This interface focuses specifically on room membership and its routines
/// are optimized for this area of room functionality.
///
struct ircd::m::room::members
{
	m::room room;

	bool test(const string_view &membership, const event::closure_bool &view) const;
	bool test(const event::closure_bool &view) const;

	void for_each(const string_view &membership, const event::closure &view) const;
	void for_each(const event::closure &view) const;

	members(m::room room)
	:room{room}
	{}
};

/// Interface to the origins (autonomous systems) of a room
///
/// This interface focuses specifically on the origins (from the field in the
/// event object) which are servers/networks/autonomous systems, or something.
/// Messages have to be sent to them, and an efficient iteration of the
/// origins as provided by this interface helps with that.
///
struct ircd::m::room::origins
{
	using closure = std::function<void (const string_view &)>;
	using closure_bool = std::function<bool (const string_view &)>;

	m::room room;

	bool test(const closure_bool &view) const;
	void for_each(const closure &view) const;

	origins(m::room room)
	:room{room}
	{}
};

/// Tuple to represent fundamental room state singletons (state_key = "")
///
/// This is not a complete representation of room state. Missing from here
/// are any state events with a state_key which is not an empty string. When
/// the state_key is not "", multiple events of that type contribute to the
/// room's eigenvalue. Additionally, state events which are not related to
/// the matrix protocol `m.room.*` are not represented here.
///
/// NOTE that in C++-land state_key="" is represented as a valid but empty
/// character pointer. It is not a string containing "". Testing for a
/// "" state_key `ircd::defined(string_view) && ircd::empty(string_view)`
/// analogous to `data() && !*data()`. A default constructed string_view{}
/// is considered "JS undefined" because the character pointers are null.
/// A "JS null" is rarer and carried with a hack which will not be discussed
/// here.
///
struct ircd::m::room::state::tuple
:json::tuple
<
	json::property<name::m_room_aliases, event>,
	json::property<name::m_room_canonical_alias, event>,
	json::property<name::m_room_create, event>,
	json::property<name::m_room_join_rules, event>,
	json::property<name::m_room_power_levels, event>,
	json::property<name::m_room_message, event>,
	json::property<name::m_room_name, event>,
	json::property<name::m_room_topic, event>,
	json::property<name::m_room_avatar, event>,
	json::property<name::m_room_pinned_events, event>,
	json::property<name::m_room_history_visibility, event>,
	json::property<name::m_room_third_party_invite, event>,
	json::property<name::m_room_guest_access, event>
>
{
	using super_type::tuple;

	tuple(const json::array &pdus);
	tuple(const m::room &, const mutable_buffer &);
	tuple() = default;
	using super_type::operator=;

	friend std::string pretty(const room::state::tuple &);
	friend std::string pretty_oneline(const room::state::tuple &);
};

#pragma GCC diagnostic pop
