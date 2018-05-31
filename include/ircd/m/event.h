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

	// General util
	bool my(const id::event &);
	bool my(const event &);
	size_t degree(const event &);
	string_view membership(const event &);
	bool check_size(std::nothrow_t, const event &);
	void check_size(const event &);

	// [GET]
	bool exists(const id::event &);
	bool bad(const id::event &, uint64_t &);
	bool bad(const id::event &);

	// Depth comparison; expect unstable sorting.
	bool operator<(const event &, const event &);
	bool operator>(const event &, const event &);
	bool operator<=(const event &, const event &);
	bool operator>=(const event &, const event &);

	// Equality tests the event_id only! know this.
	bool operator==(const event &a, const event &b);

	id::event make_id(const event &, id::event::buf &buf, const const_buffer &hash);
	id::event make_id(const event &, id::event::buf &buf);

	// Informational pretty string condensed to single line.
	std::ostream &pretty_oneline(std::ostream &, const event &, const bool &content_keys = true);
	std::string pretty_oneline(const event &, const bool &content_keys = true);

	// Informational pretty string on multiple lines.
	std::ostream &pretty(std::ostream &, const event &);
	std::string pretty(const event &);
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
	using keys = json::keys<event>;

	// Common convenience aliases
	using id = m::id::event;
	using idx = uint64_t;
	using closure = std::function<void (const event &)>;
	using closure_bool = std::function<bool (const event &)>;
	using closure_idx = std::function<void (const idx &)>;
	using closure_idx_bool = std::function<bool (const idx &)>;
	using closure_iov_mutable = std::function<void (json::iov &)>;

	static constexpr size_t MAX_SIZE = 64_KiB;
	static conf::item<size_t> max_size;

	friend event essential(event, const mutable_buffer &content);
	static void essential(json::iov &event, const json::iov &content, const closure_iov_mutable &);

	static bool verify(const string_view &, const ed25519::pk &, const ed25519::sig &sig);
	static bool verify(const json::object &, const ed25519::pk &, const ed25519::sig &sig);
	friend bool verify(const event &, const ed25519::pk &, const ed25519::sig &sig);
	friend bool verify(const event &, const ed25519::pk &, const string_view &origin, const string_view &pkid);
	friend bool verify(const event &, const string_view &origin, const string_view &pkid); // io/yield
	friend bool verify(const event &, const string_view &origin); // io/yield
	friend bool verify(const event &); // io/yield

	static ed25519::sig sign(const string_view &, const ed25519::sk &);
	static ed25519::sig sign(const string_view &);
	static ed25519::sig sign(const json::object &, const ed25519::sk &);
	static ed25519::sig sign(const json::object &);
	friend ed25519::sig sign(const event &, const ed25519::sk &);
	friend ed25519::sig sign(const event &);
	static ed25519::sig sign(json::iov &event, const json::iov &content, const ed25519::sk &);
	static ed25519::sig sign(json::iov &event, const json::iov &content);
	static json::object signatures(const mutable_buffer &, json::iov &event, const json::iov &content);
	friend event signatures(const mutable_buffer &, const m::event &);

	friend bool verify_sha256b64(const event &, const string_view &);
	friend bool verify_hash(const event &, const sha256::buf &);
	friend bool verify_hash(const event &);

	friend sha256::buf hash(const event &);
	static sha256::buf hash(json::iov &event, const string_view &content);
	static json::object hashes(const mutable_buffer &, json::iov &event, const string_view &content);
	friend json::object hashes(const mutable_buffer &, const event &);

	using super_type::tuple;
	using super_type::operator=;

	event(const idx &, const mutable_buffer &buf);
	event(const id &, const mutable_buffer &buf);
	event() = default;
};
#pragma GCC diagnostic pop

namespace ircd::m
{
	void for_each(const event::prev &, const std::function<void (const event::id &)> &);
	size_t degree(const event::prev &);
	size_t count(const event::prev &);

	std::ostream &pretty(std::ostream &, const event::prev &);
	std::string pretty(const event::prev &);

	std::ostream &pretty_oneline(std::ostream &, const event::prev &);
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
	struct opts;
	using keys = event::keys;

	static const opts default_opts;

	std::array<db::cell, event::size()> cell;
	db::row row;
	bool valid;

  public:
	fetch(const idx &, std::nothrow_t, const opts *const & = nullptr);
	fetch(const idx &, const opts *const & = nullptr);
	fetch(const id &, std::nothrow_t, const opts *const & = nullptr);
	fetch(const id &, const opts *const & = nullptr);
	fetch(const opts *const & = nullptr);

	static bool event_id(const idx &, std::nothrow_t, const id::closure &);
	static void event_id(const idx &, const id::closure &);

	friend idx index(const id &, std::nothrow_t);
	friend idx index(const id &);
	friend idx index(const event &, std::nothrow_t);
	friend idx index(const event &);

	friend bool seek(fetch &, const idx &, std::nothrow_t);
	friend void seek(fetch &, const idx &);
	friend bool seek(fetch &, const id &, std::nothrow_t);
	friend void seek(fetch &, const id &);

	using view_closure = std::function<void (const string_view &)>;
	friend bool get(std::nothrow_t, const idx &, const string_view &key, const view_closure &);
	friend bool get(std::nothrow_t, const id &, const string_view &key, const view_closure &);
	friend void get(const idx &, const string_view &key, const view_closure &);
	friend void get(const id &, const string_view &key, const view_closure &);

	friend const_buffer get(std::nothrow_t, const idx &, const string_view &key, const mutable_buffer &out);
	friend const_buffer get(std::nothrow_t, const id &, const string_view &key, const mutable_buffer &out);
	friend const_buffer get(const idx &, const string_view &key, const mutable_buffer &out);
	friend const_buffer get(const id &, const string_view &key, const mutable_buffer &out);
};

struct ircd::m::event::fetch::opts
{
	event::keys keys;
	db::gopts gopts;

	opts(const event::keys &, const db::gopts & = {});
	opts(const event::keys::selection &, const db::gopts & = {});
	opts(const db::gopts &, const event::keys::selection & = {});
	opts() = default;
};

/// Device to evaluate the conformity of an event object. This is an 'in vitro'
/// or 'pure' evaluation: it determines if the event is reasonably sane enough
/// to be evaluated further using only the information in the event itself. It
/// requires nothing external and conducts no IO etc..
///
/// This evaluation does not throw or stop when a check fails: instead it
/// collects the failures allowing the user to further determine how to proceed
/// at their own discretion.
///
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
	void operator|=(const code &) &;

	conforms() = default;
	conforms(const event &);
	conforms(const event &, const uint64_t &skip);

	static code reflect(const string_view &);
	friend string_view reflect(const code &);
	friend std::ostream &operator<<(std::ostream &, const conforms &);
};

/// Report codes corresponding to the checks conducted by event::conforms.
/// Developers: If you add a code here you must also add a string reflection
/// in the definition file.
///
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
	MISSING_SIGNATURES,                ///< no signatures
	MISSING_ORIGIN_SIGNATURE,          ///< no signature for origin
	MISMATCH_ORIGIN_SENDER,            ///< sender mxid host not from origin
	MISMATCH_ORIGIN_EVENT_ID,          ///< event_id mxid host not from origin
	SELF_REDACTS,                      ///< event redacts itself
	SELF_PREV_EVENT,                   ///< event_id self-referenced in prev_events
	SELF_PREV_STATE,                   ///< event_id self-referenced in prev_state
	DUP_PREV_EVENT,                    ///< duplicate references in prev_events
	DUP_PREV_STATE,                    ///< duplicate references in prev_state

	_NUM_
};
