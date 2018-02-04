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
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsubobject-linkage"

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

/// The _Main Event_. Most fundamental primitive of the Matrix protocol.
///
/// This json::tuple provides at least all of the legal members of the matrix
/// standard event. This is the fundamental building block of the matrix
/// system. Rooms are collections of events. Messages between servers are
/// passed as bundles of events (or directly).
///
/// Due to the ubiquitous usage, and diversity of extensions, the class
/// member interface is somewhat minimal.
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
	json::property<name::room_id, json::string>,
	json::property<name::sender, json::string>,
	json::property<name::signatures, json::object>,
	json::property<name::state_key, json::string>,
	json::property<name::type, json::string>,
	json::property<name::unsigned_, string_view>
>
{
	struct fetch;
	struct sync;
	struct prev;

	// Common convenience aliases
	using id = m::id::event;
	using closure = std::function<void (const event &)>;
	using closure_bool = std::function<bool (const event &)>;

	static database *events;

	using super_type::tuple;
	using super_type::operator=;

	event(const id &, const mutable_buffer &buf);
	event() = default;
};

namespace ircd::m
{
	void for_each(const event::prev &, const std::function<void (const event::id &)> &);
	size_t degree(const event::prev &);
	size_t count(const event::prev &);

	std::string pretty(const event::prev &);
	std::string pretty_oneline(const event::prev &);
}

struct ircd::m::event::prev
:json::tuple
<
	json::property<name::auth_events, json::array>,
	json::property<name::prev_state, json::array>,
	json::property<name::prev_events, json::array>
>
{
	enum cond :int;

	using super_type::tuple;
	using super_type::operator=;
};

inline bool
ircd::m::my(const event &event)
{
	return my(event::id(at<"event_id"_>(event)));
}

inline bool
ircd::m::my(const id::event &event_id)
{
	return self::host(event_id.host());
}

#pragma GCC diagnostic pop
