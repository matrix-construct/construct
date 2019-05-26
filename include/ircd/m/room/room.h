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

namespace ircd::m
{
	IRCD_M_EXCEPTION(m::error, CONFLICT, http::CONFLICT);
	IRCD_M_EXCEPTION(m::error, NOT_MODIFIED, http::NOT_MODIFIED);
	IRCD_M_EXCEPTION(CONFLICT, ALREADY_MEMBER, http::CONFLICT);

	struct room;

	// Util
	bool my(const room &);
	bool operator!(const room &); // room_id empty
	bool operator!=(const room &, const room &); // room_id inequality
	bool operator==(const room &, const room &); // room_id equality

	// [GET] Util
	bool exists(const room &);
	bool exists(const id::room &);
	bool exists(const id::room_alias &, const bool &remote = false);
	bool federate(const id::room &);
	id::user::buf creator(const id::room &);
	bool creator(const id::room &, const id::user &);
	string_view version(const mutable_buffer &, const room &, std::nothrow_t);
	string_view version(const mutable_buffer &, const room &);

	// [GET]
	id::room room_id(const mutable_buffer &, const id::room_alias &);
	id::room room_id(const mutable_buffer &, const string_view &id_or_alias);
	id::room::buf room_id(const id::room_alias &);
	id::room::buf room_id(const string_view &id_or_alias);

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
	event::id::buf invite(const room &, const m::id::user &target, const m::id::user &sender, json::iov &add_content);
	event::id::buf invite(const room &, const m::id::user &target, const m::id::user &sender);
	event::id::buf leave(const room &, const m::id::user &);
	event::id::buf join(const room &, const m::id::user &);
	event::id::buf join(const id::room_alias &, const m::id::user &);

	// [SET] Create new room
	room create(const createroom &, json::stack::array *const &errors = nullptr);
	room create(const id::room &, const id::user &creator, const string_view &preset = {});
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
	struct timeline;
	struct state;
	struct members;
	struct origins;
	struct head;
	struct auth;
	struct power;
	struct aliases;
	struct stats;
	struct server_acl;

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

#include "messages.h"
#include "timeline.h"
#include "state.h"
#include "state_space.h"
#include "members.h"
#include "origins.h"
#include "head.h"
#include "auth.h"
#include "power.h"
#include "aliases.h"
#include "stats.h"
#include "server_acl.h"

inline ircd::m::room::operator
const ircd::m::room::id &()
const
{
	return room_id;
}
