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

	// Util
	bool my(const room &);

	// [GET] Util
	bool exists(const room &);
	bool exists(const id::room &);
	bool exists(const id::room_alias &, const bool &remote = false);
	uint version(const id::room &);
	bool federate(const id::room &);

	// [GET]
	id::room room_id(const mutable_buffer &, const id::room_alias &);
	id::room room_id(const mutable_buffer &, const string_view &id_or_alias);
	id::room::buf room_id(const id::room_alias &);
	id::room::buf room_id(const string_view &id_or_alias);

	// [GET] Current Event ID and depth suite (non-locking) (one only)
	std::tuple<id::event::buf, int64_t, event::idx> top(std::nothrow_t, const id::room &);
	std::tuple<id::event::buf, int64_t, event::idx> top(const id::room &);

	// [GET] Current Event ID (non-locking) (one only)
	id::event::buf head(std::nothrow_t, const id::room &);
	id::event::buf head(const id::room &);

	// [GET] Current Event idx (non-locking) (one only)
	event::idx head_idx(std::nothrow_t, const id::room &);
	event::idx head_idx(const id::room &);

	// [GET] Current Event depth (non-locking)
	int64_t depth(std::nothrow_t, const id::room &);
	int64_t depth(const id::room &);

	// [GET] Count the events in the room between two indexes.
	size_t count_since(const room &, const m::event::idx &, const m::event::idx &);
	size_t count_since(const room &, const m::event::id &, const m::event::id &);
	size_t count_since(const m::event::idx &, const m::event::idx &);
	size_t count_since(const m::event::id &, const m::event::id &);

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
	event::id::buf redact(const room &, const m::id::user &sender, const m::id::event &, const string_view &reason);
	event::id::buf message(const room &, const m::id::user &sender, const json::members &content);
	event::id::buf message(const room &, const m::id::user &sender, const string_view &body, const string_view &msgtype = "m.text");
	event::id::buf msghtml(const room &, const m::id::user &sender, const string_view &html, const string_view &alt = {}, const string_view &msgtype = "m.notice");
	event::id::buf notice(const room &, const m::id::user &sender, const string_view &body);
	event::id::buf notice(const room &, const string_view &body); // sender is @ircd
	event::id::buf invite(const room &, const m::id::user &target, const m::id::user &sender);
	event::id::buf leave(const room &, const m::id::user &);
	event::id::buf join(const room &, const m::id::user &);
	event::id::buf join(const id::room_alias &, const m::id::user &);

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
	struct power;

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

	// Convenience passthru to room::messages (linear query)
	bool get(std::nothrow_t, const string_view &type, const event::closure &) const;
	void get(const string_view &type, const event::closure &) const;
	bool has(const string_view &type) const;

	// misc
	bool membership(const m::id::user &, const string_view &membership = "join") const;
	string_view membership(const mutable_buffer &out, const m::id::user &) const;
	bool visible(const string_view &mxid, const m::event *const & = nullptr) const;
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
};

/// Interface to room messages
///
/// This interface has the form of an STL-style iterator over room messages
/// which are state and non-state events from all integrated timelines.
/// Moving the iterator is cheap, but the dereference operators fetch a
/// full event. One can iterate just event_idx's by using event_idx() instead
/// of the dereference operators.
///
struct ircd::m::room::messages
{
	m::room room;
	db::index::const_iterator it;
	event::idx _event_idx;
	event::fetch _event;

  public:
	operator bool() const              { return bool(it);                      }
	bool operator!() const             { return !it;                           }

	event::id::buf event_id();         // deprecated; will remove
	const event::idx &event_idx();
	string_view state_root() const;

	const m::event &fetch(std::nothrow_t);
	const m::event &fetch();

	bool seek_idx(const event::idx &);
	bool seek(const uint64_t &depth);
	bool seek(const event::id &);
	bool seek();

	// These are reversed on purpose
	auto &operator++()                 { return --it;                          }
	auto &operator--()                 { return ++it;                          }

	const m::event &operator*();
	const m::event *operator->()       { return &operator*();                  }

	messages(const m::room &room,
	         const uint64_t &depth,
	         const event::fetch::opts *const & = nullptr);

	messages(const m::room &room,
	         const event::id &,
	         const event::fetch::opts *const & = nullptr);

	messages(const m::room &room,
	         const event::fetch::opts *const & = nullptr);

	messages() = default;
	messages(const messages &) = delete;
	messages &operator=(const messages &) = delete;
};

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
	using types = std::function<bool (const string_view &)>;
	using keys_bool = std::function<bool (const string_view &)>;

	room::id room_id;
	event::id::buf event_id;
	m::state::id_buffer root_id_buf;
	m::state::id root_id;
	const event::fetch::opts *fopts {nullptr};

	bool present() const;

	// Iterate the state; for_each protocol
	bool for_each(const types &) const;
	void for_each(const string_view &type, const keys &) const;
	void for_each(const string_view &type, const event::closure_idx &) const;
	void for_each(const string_view &type, const event::id::closure &) const;
	void for_each(const string_view &type, const event::closure &) const;
	void for_each(const event::closure_idx &) const;
	void for_each(const event::id::closure &) const;
	void for_each(const event::closure &) const;

	// Iterate the state; test protocol
	bool test(const types &) const;
	bool test(const string_view &type, const string_view &lower_bound, const event::closure_idx_bool &view) const;
	bool test(const string_view &type, const string_view &lower_bound, const event::id::closure_bool &view) const;
	bool test(const string_view &type, const string_view &lower_bound, const event::closure_bool &view) const;
	bool test(const string_view &type, const keys_bool &view) const;
	bool test(const string_view &type, const event::closure_idx_bool &view) const;
	bool test(const string_view &type, const event::id::closure_bool &view) const;
	bool test(const string_view &type, const event::closure_bool &view) const;
	bool test(const event::closure_idx_bool &view) const;
	bool test(const event::id::closure_bool &view) const;
	bool test(const event::closure_bool &view) const;

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

	state(const m::room &room, const event::fetch::opts *const & = nullptr);
	state() = default;
	state(const state &) = delete;
	state &operator=(const state &) = delete;
};

/// Interface to the members of a room.
///
/// This interface focuses specifically on room membership and its routines
/// are optimized for this area of room functionality.
///
struct ircd::m::room::members
{
	using closure = std::function<void (const id::user &)>;
	using closure_bool = std::function<bool (const id::user &)>;

	m::room room;

	size_t count() const;
	size_t count(const string_view &membership) const;

	bool test(const string_view &membership, const event::closure_bool &view) const;
	bool test(const event::closure_bool &view) const;

	void for_each(const string_view &membership, const event::closure &) const;
	bool for_each(const string_view &membership, const closure_bool &) const;
	void for_each(const string_view &membership, const closure &) const;
	void for_each(const event::closure &) const;
	bool for_each(const closure_bool &) const;
	void for_each(const closure &) const;

	members(const m::room &room)
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

	bool _test_(const closure_bool &view) const;
	bool test(const closure_bool &view) const;
	bool for_each(const closure_bool &view) const;
	void for_each(const closure &view) const;
	bool has(const string_view &origin) const;
	bool only(const string_view &origin) const;
	size_t count() const;

	// select an origin in the room at random; use proffer to refuse and try another.
	string_view random(const mutable_buffer &buf, const closure_bool &proffer = nullptr) const;
	bool random(const closure &, const closure_bool &proffer = nullptr) const;

	origins(const m::room &room)
	:room{room}
	{}
};

/// Interface to the room head
///
/// This interface helps compute and represent aspects of the room graph,
/// specifically concerning the "head" or the "front" or the "top" of this
/// graph where events are either furthest from the m.room.create genesis,
/// or are yet unreferenced by another event. Usage of this interface is
/// fundamental when composing the references of a new event on the graph.
///
struct ircd::m::room::head
{
	using closure = std::function<void (const event::idx &, const event::id &)>;
	using closure_bool = std::function<bool (const event::idx &, const event::id &)>;

	m::room room;

	bool for_each(const closure_bool &) const;
	void for_each(const closure &) const;
	bool has(const event::id &) const;
	size_t count() const;

	head(const m::room &room)
	:room{room}
	{}
};

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

	static const int64_t default_power_level;
	static const int64_t default_event_level;
	static const int64_t default_user_level;

	m::room room;
	m::room::state state;

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
	// always return suitable results.
	int64_t level(const string_view &prop) const;
	int64_t level_event(const string_view &type) const;
	int64_t level_user(const m::id::user &) const;

	// all who attain great power and riches make use of either force or fraud"
	bool operator()(const m::id::user &, const string_view &prop, const string_view &type = {}) const;

	power(const m::room &room)
	:room{room}
	,state{room}
	{}
};

inline ircd::m::room::operator
const ircd::m::room::id &()
const
{
	return room_id;
}
