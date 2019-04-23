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
	// event/event.h
	struct event;

	// General util
	bool my(const id::event &);
	bool my(const event &);
	size_t degree(const event &);
	string_view membership(const event &);

	// [GET]
	bool exists(const id::event &);
	bool exists(const id::event &, const bool &good);
	bool cached(const id::event &);
	bool good(const id::event &);
	bool bad(const id::event &);

	// Equality tests the event_id only! know this.
	bool operator==(const event &a, const event &b);

	// Depth comparison; expect unstable sorting.
	bool operator<(const event &, const event &);
	bool operator>(const event &, const event &);
	bool operator<=(const event &, const event &);
	bool operator>=(const event &, const event &);

	// Topological comparison
	bool before(const event &a, const event &b);

	id::event make_id(const event &, id::event::buf &buf, const const_buffer &hash);
	id::event make_id(const event &, id::event::buf &buf);

	json::object hashes(const mutable_buffer &, const event &);
	event signatures(const mutable_buffer &, const m::event &);
	event essential(event, const mutable_buffer &content);

	bool verify_sha256b64(const event &, const string_view &);
	bool verify_hash(const event &, const sha256::buf &);
	bool verify_hash(const event &);

	bool verify(const event &, const ed25519::pk &, const ed25519::sig &sig);
	bool verify(const event &, const ed25519::pk &, const string_view &origin, const string_view &pkid);
	bool verify(const event &, const string_view &origin, const string_view &pkid); // io/yield
	bool verify(const event &, const string_view &origin); // io/yield
	bool verify(const event &); // io/yield

	sha256::buf hash(const event &);
	ed25519::sig sign(const event &, const ed25519::sk &);
	ed25519::sig sign(const event &);
}

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
	struct refs;
	struct auth;
	struct fetch;
	struct conforms;

	using keys = json::keys<event>;
	using id = m::id::event;
	using idx = uint64_t;
	using idx_range = std::pair<idx, idx>;
	using closure = std::function<void (const event &)>;
	using closure_bool = std::function<bool (const event &)>;
	using closure_idx = std::function<void (const idx &)>;
	using closure_idx_bool = std::function<bool (const idx &)>;
	using closure_iov_mutable = std::function<void (json::iov &)>;

	static constexpr const size_t MAX_SIZE = 64_KiB;
	static constexpr const size_t TYPE_MAX_SIZE = 256;
	static constexpr const size_t STATE_KEY_MAX_SIZE = 256;
	static constexpr const size_t ORIGIN_MAX_SIZE = 256;
	static conf::item<size_t> max_size;

	static void essential(json::iov &event, const json::iov &content, const closure_iov_mutable &);
	static bool verify(const json::object &, const ed25519::pk &, const ed25519::sig &sig);
	static ed25519::sig sign(const string_view &, const ed25519::sk &);
	static ed25519::sig sign(const string_view &);
	static ed25519::sig sign(const json::object &, const ed25519::sk &);
	static ed25519::sig sign(const json::object &);
	static ed25519::sig sign(json::iov &event, const json::iov &content, const ed25519::sk &);
	static ed25519::sig sign(json::iov &event, const json::iov &content);
	static json::object signatures(const mutable_buffer &, json::iov &event, const json::iov &content);
	static sha256::buf hash(json::iov &event, const string_view &content);
	static sha256::buf hash(const json::object &);
	static json::object hashes(const mutable_buffer &, json::iov &event, const string_view &content);

	json::object source; // Contextual availability only.

	using super_type::tuple;
	event(const json::object &);
	event(const json::object &, const keys &);
	event() = default;
};

#include "event/prev.h"
#include "event/refs.h"
#include "event/auth.h"
#include "event/index.h"
#include "event/event_id.h"
#include "event/fetch.h"
#include "event/get.h"
#include "event/cached.h"
#include "event/prefetch.h"
#include "event/conforms.h"
#include "event/append.h"
#include "event/pretty.h"
