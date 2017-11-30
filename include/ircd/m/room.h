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
#define HAVE_IRCD_M_ROOM_H
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsubobject-linkage"

namespace ircd::m
{
	IRCD_M_EXCEPTION(m::error, CONFLICT, http::CONFLICT);
	IRCD_M_EXCEPTION(m::error, NOT_MODIFIED, http::NOT_MODIFIED);
	IRCD_M_EXCEPTION(CONFLICT, ALREADY_MEMBER, http::CONFLICT);

	struct room;

	bool exists(const id::room &);

	event::id::buf send(const id::room &, const m::id::user &sender, const string_view &type, const json::iov &content);
	event::id::buf send(const id::room &, const m::id::user &sender, const string_view &type, const json::members &content);
	event::id::buf send(const id::room &, const m::id::user &sender, const string_view &type, const string_view &state_key, const json::iov &content);
	event::id::buf send(const id::room &, const m::id::user &sender, const string_view &type, const string_view &state_key, const json::members &content);

	event::id::buf message(const id::room &, const m::id::user &sender, const json::members &content);
	event::id::buf message(const id::room &, const m::id::user &sender, const string_view &body, const string_view &msgtype = "m.text");
	event::id::buf membership(const id::room &, const m::id::user &, const string_view &membership);
	event::id::buf leave(const id::room &, const m::id::user &);
	event::id::buf join(const id::room &, const m::id::user &);

	room create(const id::room &, const id::user &creator, const id::room &parent, const string_view &type);
	room create(const id::room &, const id::user &creator, const string_view &type = {});
}

struct ircd::m::room
{
	struct alias;
	struct state;
	struct members;
	struct fetch;
	struct branch;

	using id = m::id::room;

	id room_id;

	operator const id &() const        { return room_id;                       }

	// observer
	bool test(const string_view &type, const event::closure_bool &view) const;
	void for_each(const string_view &type, const event::closure &view) const;
	bool has(const string_view &type, const string_view &state_key) const;
	bool has(const string_view &type) const;
	bool get(const string_view &type, const string_view &state_key, const event::closure &) const;
	bool get(const string_view &type, const event::closure &view) const;

	// observer misc
	bool membership(const m::id::user &, const string_view &membership = "join") const;
	uint64_t maxdepth(event::id::buf &) const;
	uint64_t maxdepth() const;

	// modify
	event::id::buf send(json::iov &event);
	event::id::buf send(json::iov &event, const json::iov &content);

	// modify misc
	event::id::buf message(json::iov &event, const json::iov &content);
	event::id::buf membership(json::iov &event, const json::iov &content);
	event::id::buf create(json::iov &event, json::iov &content);

	room(const id::alias &alias);
	room(const id &room_id)
	:room_id{room_id}
	{}
};

namespace ircd::m::name
{
	constexpr const char *const m_room_aliases {"m.room.aliases"};
	constexpr const char *const m_room_canonical_alias {"m.room.canonical_alias"};
	constexpr const char *const m_room_create {"m.room.create"};
	constexpr const char *const m_room_join_rules {"m.room.join_rules"};
	constexpr const char *const m_room_member {"m.room.member"};
	constexpr const char *const m_room_power_levels {"m.room.power_levels"};
	constexpr const char *const m_room_message {"m.room.message"};
	constexpr const char *const m_room_name {"m.room.name"};
	constexpr const char *const m_room_topic {"m.room.topic"};
	constexpr const char *const m_room_avatar {"m.room.avatar"};
	constexpr const char *const m_room_pinned_events {"m.room.pinned_events"};
	constexpr const char *const m_room_history_visibility {"m.room.history_visibility"};
	constexpr const char *const m_room_third_party_invite {"m.room.third_party_invite"};
	constexpr const char *const m_room_guest_access {"m.room.guest_access"};
}

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
struct ircd::m::room::state
:json::tuple
<
	json::property<m::name::m_room_aliases, event>,
	json::property<m::name::m_room_canonical_alias, event>,
	json::property<m::name::m_room_create, event>,
	json::property<m::name::m_room_join_rules, event>,
	json::property<m::name::m_room_power_levels, event>,
	json::property<m::name::m_room_message, event>,
	json::property<m::name::m_room_name, event>,
	json::property<m::name::m_room_topic, event>,
	json::property<m::name::m_room_avatar, event>,
	json::property<m::name::m_room_pinned_events, event>,
	json::property<m::name::m_room_history_visibility, event>,
	json::property<m::name::m_room_third_party_invite, event>,
	json::property<m::name::m_room_guest_access, event>
>
{
	struct fetch;

	using super_type::tuple;

	state(const json::array &pdus);
	state(fetch &);
	state(const room::id &, const event::id &, const mutable_buffer &);
	state() = default;
	using super_type::operator=;

	friend std::string pretty(const room::state &);
	friend std::string pretty_oneline(const room::state &);
};

struct ircd::m::room::branch
{
	event::id::buf event_id;
	unique_buffer<mutable_buffer> buf;
	json::object pdu;

	branch() = default;
	branch(const event::id &event_id)
	:event_id{event_id}
	,buf{64_KiB}
	,pdu{}
	{}
};

#pragma GCC diagnostic pop
