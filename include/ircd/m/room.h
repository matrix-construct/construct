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

namespace ircd::m
{
	IRCD_M_EXCEPTION(m::error, CONFLICT, http::CONFLICT);
	IRCD_M_EXCEPTION(m::error, NOT_MODIFIED, http::NOT_MODIFIED);
	IRCD_M_EXCEPTION(CONFLICT, ALREADY_MEMBER, http::CONFLICT);

	struct room;
}

namespace ircd::m::rooms
{
}

struct ircd::m::room
{
	struct alias;

	string_view room_id;

	using event_closure = std::function<void (const event &)>;
	void for_each(const events::where &, const event_closure &) const;
	void for_each(const event_closure &) const;

	using event_closure_bool = std::function<bool (const event &)>;
	size_t count(const events::where &, const event_closure_bool &) const;
	size_t count(const events::where &) const;
	bool any(const events::where &, const event_closure_bool &) const;
	bool any(const events::where &) const;

	bool is_member(const m::id::user &, const string_view &membership = "join");

	void membership(const m::id::user &, const json::builder &content);
	void join(const m::id::user &, const json::builder &content);

	room(const id::room &room_id);
	room(const id::alias &alias);
};

inline
ircd::m::room::room(const id::room &room_id)
:room_id{room_id}
{}

inline
ircd::m::room::room(const id::alias &alias)
:room_id{}
{}
