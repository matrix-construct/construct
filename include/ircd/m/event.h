/*
 * charybdis: 21st Century IRC++d
 *
 * Copyright (C) 2016 Charybdis Development Team
 * Copyright (C) 2016 Jason Volk <jason@zemos.net>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice is present in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#pragma once
#define HAVE_IRCD_M_EVENT_H
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsubobject-linkage"

///////////////////////////////////////////////////////////////////////////////
// Protocol notes
//
// 10.4
// The total size of any event MUST NOT exceed 65 KB.
//
// There are additional restrictions on sizes per key:
// sender MUST NOT exceed 255 bytes (including domain).
// room_id MUST NOT exceed 255 bytes.
// state_key MUST NOT exceed 255 bytes.
// type MUST NOT exceed 255 bytes.
// event_id MUST NOT exceed 255 bytes.
//
// Some event types have additional size restrictions which are specified in
// the description of the event. Additional keys have no limit other than that
// implied by the total 65 KB limit on events.
//

namespace ircd::m
{
	struct event;

	bool my(const id::event &);
	bool my(const event &);

	size_t degree(const event &);
	std::string pretty(const event &);
	std::string pretty_oneline(const event &);
}

namespace ircd::m::name
{
	constexpr const char *const auth_events {"auth_events"};
	constexpr const char *const content {"content"};
	constexpr const char *const depth {"depth"};
	constexpr const char *const event_id {"event_id"};
	constexpr const char *const hashes {"hashes"};
	constexpr const char *const is_state {"is_state"};
	constexpr const char *const membership {"membership"};
	constexpr const char *const origin {"origin"};
	constexpr const char *const origin_server_ts {"origin_server_ts"};
	constexpr const char *const prev_events {"prev_events"};
	constexpr const char *const prev_state {"prev_state"};
	constexpr const char *const room_id {"room_id"};
	constexpr const char *const sender {"sender"};
	constexpr const char *const signatures {"signatures"};
	constexpr const char *const state_key {"state_key"};
	constexpr const char *const type {"type"};
	constexpr const char *const unsigned_ {"unsigned"};
}

/// The _Main Event_. Most fundamental primitive of the Matrix protocol.
///
/// This json::tuple provides at least all of the legal members of the matrix
/// standard event. This is the fundamental building block of the matrix
/// system. Rooms are collections of events. Messages between servers are
/// passed as bundles of events (or directly). Due to the ubiquitous usage,
/// and diversity of extensions, the class member interface is somewhat minimal.
///
struct ircd::m::event
:json::tuple
<
	json::property<name::auth_events, json::array>,
	json::property<name::content, json::object>,
	json::property<name::depth, int64_t>,
	json::property<name::event_id, json::string>,
	json::property<name::hashes, json::object>,
	json::property<name::is_state, bool>,
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
	enum lineage : int;
	enum temporality : int;

	struct fetch;
	struct sync;
	struct prev;

	// Common convenience aliases
	using id = m::id::event;
	using closure = std::function<void (const event &)>;
	using closure_bool = std::function<bool (const event &)>;

	static database *events;

	using super_type::tuple;
	event(const id &, const mutable_buffer &buf);
	event(fetch &);
	event() = default;
	using super_type::operator=;
};

namespace ircd::m
{
	void for_each(const event::prev &, const std::function<void (const event::id &)> &);
	size_t degree(const event::prev &);
	size_t count(const event::prev &);

	std::string pretty(const event::prev &);
	std::string pretty_oneline(const event::prev &);

	string_view reflect(const event::temporality &);
	string_view reflect(const event::lineage &);

	event::temporality temporality(const event &, const int64_t &rel);
	event::lineage lineage(const event &);
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

enum ircd::m::event::prev::cond
:int
{
	SELF_LOOP,
};

enum ircd::m::event::temporality
:int
{
	FUTURE      = 1,   ///< Event has a depth 1 or more into the future.
	PRESENT     = 0,   ///< Event has a depth equal to the current depth.
	PAST        = -1,  ///< Event has a depth less than the current depth.
};

enum ircd::m::event::lineage
:int
{
	ROOT        = 0,   ///< Event has no parents (must be m.room.create then)
	FORWARD     = 1,   ///< Event has one parent at the previous depth
	MERGE       = 2,   ///< Event has multiple parents at the previous depth
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
