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

	room create(const id::room &room_id, const id::user &creator, const id::room &parent, const string_view &type);
	room create(const id::room &room_id, const id::user &creator, const string_view &type = {});

	void membership(const id::room &room_id, const m::id::user &, const string_view &membership);
	void leave(const id::room &room_id, const m::id::user &);
	void join(const id::room &room_id, const m::id::user &);
}

struct ircd::m::room
{
	struct alias;
	struct state;
	struct members;
	struct fetch;

	using id = m::id::room;

	id room_id;

	operator const id &() const
	{
		return room_id;
	}

	bool membership(const m::id::user &, const string_view &membership = "join") const;

	std::vector<std::string> barren(const int64_t &min_depth = 0) const;
	uint64_t maxdepth(event::id::buf &) const;
	uint64_t maxdepth() const;

	event::id::buf send(json::iov &event);
	event::id::buf send(const json::members &event);

	void membership(json::iov &event, json::iov &content);
	void create(json::iov &event, json::iov &content);

	room(const id &room_id)
	:room_id{room_id}
	{}

	room(const id::alias &alias)
	:room_id{}
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
	constexpr const char *const m_room_redaction {"m.room.redaction"};
	constexpr const char *const m_room_message {"m.room.message"};
	constexpr const char *const m_room_name {"m.room.name"};
	constexpr const char *const m_room_topic {"m.room.topic"};
	constexpr const char *const m_room_avatar {"m.room.avatar"};
}

struct ircd::m::room::state
:json::tuple
<
	json::property<m::name::m_room_aliases, event>,
	json::property<m::name::m_room_canonical_alias, event>,
	json::property<m::name::m_room_create, event>,
	json::property<m::name::m_room_join_rules, event>,
	json::property<m::name::m_room_member, event>,
	json::property<m::name::m_room_power_levels, event>,
	json::property<m::name::m_room_redaction, event>,
	json::property<m::name::m_room_message, event>,
	json::property<m::name::m_room_name, event>,
	json::property<m::name::m_room_topic, event>,
	json::property<m::name::m_room_avatar, event>
>
{
	struct fetch;
};

#pragma GCC diagnostic pop
