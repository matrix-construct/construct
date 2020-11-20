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
	struct room;

	IRCD_M_EXCEPTION(m::error, CONFLICT, http::CONFLICT);
	IRCD_M_EXCEPTION(m::error, NOT_MODIFIED, http::NOT_MODIFIED);
	IRCD_M_EXCEPTION(CONFLICT, ALREADY_MEMBER, http::CONFLICT);

	// Util
	bool my(const room &);
	bool operator!(const room &); // room_id empty
	bool operator!=(const room &, const room &); // room_id inequality
	bool operator==(const room &, const room &); // room_id equality

	// [GET] Convenience boolean suite
	bool exists(const room &);
	bool exists(const id::room &);
	bool exists(const id::room_alias &, const bool &remote = false);
	bool internal(const id::room &);
	bool federated(const id::room &);
	bool creator(const id::room &, const id::user &);
	bool contains(const id::room &, const event::idx &);
	bool membership(const room &, const id::user &, const string_view & = "join");
	bool join_rule(const room &, const string_view &rule);
	bool visible(const room &, const string_view &mxid, const m::event *const & = nullptr);
	bool remote_joined(const room &);
	bool local_joined(const room &);
	bool local_only(const room &);

	// [GET] Convenience and tools
	id::user::buf creator(const id::room &);
	string_view type(const mutable_buffer &, const room &);
	string_view version(const mutable_buffer &, const room &, std::nothrow_t);
	string_view version(const mutable_buffer &, const room &);
	string_view join_rule(const mutable_buffer &out, const room &);
	string_view membership(const mutable_buffer &out, const room &, const m::id::user &);
	string_view display_name(const mutable_buffer &out, const room &);
	id::user::buf any_user(const room &, const string_view &host, const string_view &memshp = "join");

	id::room room_id(const mutable_buffer &, const event::idx &); // db
	id::room room_id(const mutable_buffer &, const id::event &); // db
	id::room room_id(const mutable_buffer &, const id::room_alias &); // db + net
	id::room room_id(const mutable_buffer &, const string_view &mxid);
	id::room::buf room_id(const event::idx &); // db
	id::room::buf room_id(const id::event &); // db
	id::room::buf room_id(const id::room_alias &); // db + net
	id::room::buf room_id(const string_view &id_or_alias);

	// [SET] Lowest-level
	event::id::buf commit(const room &, json::iov &event, const json::iov &content);

	// [SET] Send state to room
	event::id::buf send(const room &, const id::user &sender, const string_view &type, const string_view &state_key, const json::iov &content);
	event::id::buf send(const room &, const id::user &sender, const string_view &type, const string_view &state_key, const json::members &content);
	event::id::buf send(const room &, const id::user &sender, const string_view &type, const string_view &state_key, const json::object &content);

	// [SET] Send non-state to room
	event::id::buf send(const room &, const id::user &sender, const string_view &type, const json::iov &content);
	event::id::buf send(const room &, const id::user &sender, const string_view &type, const json::members &content);
	event::id::buf send(const room &, const id::user &sender, const string_view &type, const json::object &content);

	// [SET] Convenience sends
	event::id::buf react(const room &, const id::user &sender, const id::event &, const string_view &rel_type, json::iov &relates);
	event::id::buf annotate(const room &, const id::user &sender, const id::event &, const string_view &key);
	event::id::buf message(const room &, const id::user &sender, const json::members &content);
	event::id::buf message(const room &, const id::user &sender, const string_view &body, const string_view &msgtype = "m.text");
	event::id::buf msghtml(const room &, const id::user &sender, const string_view &html, const string_view &alt = {}, const string_view &msgtype = "m.notice");
	event::id::buf notice(const room &, const id::user &sender, const string_view &body);
	event::id::buf notice(const room &, const string_view &body); // sender is @ircd
	event::id::buf redact(const room &, const id::user &sender, const id::event &, const string_view &reason);
	event::id::buf invite(const room &, const id::user &target, const id::user &sender, json::iov &add_content);
	event::id::buf invite(const room &, const id::user &target, const id::user &sender);
	event::id::buf leave(const room &, const id::user &);
	event::id::buf join(const room &, const id::user &, const vector_view<const string_view> &remotes = {});
	event::id::buf join(const id::room_alias &, const id::user &);

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
	struct events;
	struct state;
	struct members;
	struct origins;
	struct type;
	struct head;
	struct auth;
	struct power;
	struct aliases;
	struct stats;
	struct server_acl;
	struct message;
	struct bootstrap;
	struct content;

	using id = m::id::room;
	using alias = m::id::room_alias;
	using closure = std::function<void (const room &)>;
	using closure_bool = std::function<bool (const room &)>;

	static constexpr const size_t &VERSION_MAX_SIZE {32};
	static constexpr const size_t &MEMBERSHIP_MAX_SIZE {16};

	id room_id;
	event::id event_id;
	const vm::copts *copts {nullptr};
	const event::fetch::opts *fopts {nullptr};

	operator const id &() const;

	// Convenience passthru to room::events (linear query; newest first)
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
	event::idx get(std::nothrow_t, const string_view &type) const;
	event::idx get(const string_view &type) const;
	size_t count(const string_view &type, const string_view &state_key) const;
	size_t count(const string_view &type) const;
	size_t count() const;

	room(const id &room_id,
	     const string_view &event_id,
	     const vm::copts *const &copts = nullptr,
	     const event::fetch::opts *const &fopts = nullptr);

	room(const id &room_id = {},
	     const vm::copts *const &copts = nullptr,
	     const event::fetch::opts *const &fopts = nullptr) noexcept;

	// Index of create event
	static event::idx index(const id &, std::nothrow_t);
	static event::idx index(const id &);

	static size_t purge(const room &); // cuidado!
};

#include "events.h"
#include "state.h"
#include "state_space.h"
#include "state_history.h"
#include "state_fetch.h"
#include "members.h"
#include "origins.h"
#include "type.h"
#include "content.h"
#include "head.h"
#include "head_generate.h"
#include "head_fetch.h"
#include "auth.h"
#include "power.h"
#include "aliases.h"
#include "stats.h"
#include "server_acl.h"
#include "message.h"
#include "bootstrap.h"

inline
ircd::m::room::room(const id &room_id,
                    const string_view &event_id,
                    const vm::copts *const &copts,
                    const event::fetch::opts *const &fopts)
:room_id{room_id}
,event_id{event_id? event::id{event_id} : event::id{}}
,copts{copts}
,fopts{fopts}
{}

inline
ircd::m::room::room(const id &room_id,
                    const vm::copts *const &copts,
                    const event::fetch::opts *const &fopts)
noexcept
:room_id{room_id}
,copts{copts}
,fopts{fopts}
{}

inline ircd::m::room::operator
const ircd::m::room::id &()
const
{
	return room_id;
}
