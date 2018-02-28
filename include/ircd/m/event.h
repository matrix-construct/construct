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
#define HAVE_IRCD_M_EVENT_H

namespace ircd::m
{
	struct event;

	bool my(const id::event &);
	bool my(const event &);

	size_t degree(const event &);

	std::string pretty(const event &);
	std::string pretty_oneline(const event &);

	id::event event_id(const event &, id::event::buf &buf, const const_buffer &hash);
	id::event event_id(const event &, id::event::buf &buf);
	id::event event_id(const event &);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsubobject-linkage"
/// The Main Event
///
/// This json::tuple provides at least all of the legal members of the matrix
/// standard event. This is the fundamental building block of the matrix
/// system. Rooms are collections of events. Messages between servers are
/// passed as bundles of events (or directly).
///
/// It is better to have 100 functions operate on one data structure than
/// to have 10 functions operate on 10 data structures.
/// -Alan Perlis
///
struct ircd::m::event
:json::tuple
<
	json::property<name::auth_events, json::array>,
	json::property<name::content, json::object>,
	json::property<name::depth, int64_t>,
	json::property<name::event_id, json::string>,
	json::property<name::hashes, json::object>,
	json::property<name::membership, json::string>,
	json::property<name::origin, json::string>,
	json::property<name::origin_server_ts, time_t>,
	json::property<name::prev_events, json::array>,
	json::property<name::prev_state, json::array>,
	json::property<name::redacts, json::string>,
	json::property<name::room_id, json::string>,
	json::property<name::sender, json::string>,
	json::property<name::signatures, json::object>,
	json::property<name::state_key, json::string>,
	json::property<name::type, json::string>
>
{
	struct prev;
	struct fetch;
	struct conforms;

	// Common convenience aliases
	using id = m::id::event;
	using closure = std::function<void (const event &)>;
	using closure_bool = std::function<bool (const event &)>;

	using super_type::tuple;
	using super_type::operator=;

	event(const id &, const mutable_buffer &buf);
	event() = default;
};
#pragma GCC diagnostic pop

namespace ircd::m
{
	void for_each(const event::prev &, const std::function<void (const event::id &)> &);
	size_t degree(const event::prev &);
	size_t count(const event::prev &);

	std::string pretty(const event::prev &);
	std::string pretty_oneline(const event::prev &);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsubobject-linkage"
struct ircd::m::event::prev
:json::tuple
<
	json::property<name::auth_events, json::array>,
	json::property<name::prev_state, json::array>,
	json::property<name::prev_events, json::array>
>
{
	enum cond :int;

	std::tuple<event::id, string_view> auth_events(const uint &idx) const;
	std::tuple<event::id, string_view> prev_states(const uint &idx) const;
	std::tuple<event::id, string_view> prev_events(const uint &idx) const;

	event::id auth_event(const uint &idx) const;
	event::id prev_state(const uint &idx) const;
	event::id prev_event(const uint &idx) const;

	using super_type::tuple;
	using super_type::operator=;
};
#pragma GCC diagnostic pop

struct ircd::m::event::fetch
:event
{
	std::array<db::cell, event::size()> cell;
	db::row row;

	bool valid(const event::id &) const;

	fetch(const event::id &, std::nothrow_t);
	fetch(const event::id &);
	fetch();

	friend bool seek(fetch &, const event::id &, std::nothrow_t);
	friend void seek(fetch &, const event::id &);
};

struct ircd::m::event::conforms
{
	enum code :uint;

	uint64_t report {0};

	bool clean() const;
	operator bool() const;
	bool operator!() const;
	bool has(const uint &code) const;
	bool has(const code &code) const;
	string_view string(const mutable_buffer &out) const;

	void set(const code &code);
	void del(const code &code);

	conforms() = default;
	conforms(const event &);
	conforms(const event &, const uint64_t &skip);

	static code reflect(const string_view &);
	friend string_view reflect(const code &);
	friend std::ostream &operator<<(std::ostream &, const conforms &);
};

enum ircd::m::event::conforms::code
:uint
{
	INVALID_OR_MISSING_EVENT_ID,       ///< event_id empty or failed mxid grammar check
	INVALID_OR_MISSING_ROOM_ID,        ///< room_id empty or failed mxid grammar check
	INVALID_OR_MISSING_SENDER_ID,      ///< sender empty or failed mxid grammar check
	MISSING_TYPE,                      ///< type empty
	MISSING_ORIGIN,                    ///< origin empty
	INVALID_ORIGIN,                    ///< origin not a proper domain
	INVALID_OR_MISSING_REDACTS_ID,     ///< for m.room.redaction
	MISSING_MEMBERSHIP,                ///< for m.room.member, membership empty
	INVALID_MEMBERSHIP,                ///< for m.room.member (does not check actual states)
	MISSING_CONTENT_MEMBERSHIP,        ///< for m.room.member, content.membership
	INVALID_CONTENT_MEMBERSHIP,        ///< for m.room.member, content.membership
	MISSING_PREV_EVENTS,               ///< for non-m.room.create, empty prev_events
	MISSING_PREV_STATE,                ///< for state_key'ed, empty prev_state
	DEPTH_NEGATIVE,                    ///< depth < 0
	DEPTH_ZERO,                        ///< for non-m.room.create, depth=0

	_NUM_
};
