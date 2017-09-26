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

struct ircd::m::room
{
	struct alias;
	struct events;
	struct state;

	using id = m::id::room;

	id room_id;

	event::id::buf send(json::iov &event);
	event::id::buf send(const json::members &event);

	bool is_member(const m::id::user &, const string_view &membership = "join");
	void membership(const m::id::user &, json::iov &content);
	void leave(const m::id::user &, json::iov &content);
	void join(const m::id::user &, json::iov &content);
	void create(const m::id::user &sender, const m::id::user &creator, json::iov &content);

	room(const id &room_id)
	:room_id{room_id}
	{}

	room(const id::alias &alias)
	:room_id{}
	{}
};

struct ircd::m::room::events
:m::events
{
	id room_id;

	bool _query_(const event::where &, const event_closure_bool &) const override;
	bool _rquery_(const event::where &, const event_closure_bool &) const override;

	events(const id &room_id)
	:room_id{room_id}
	{}

	events(const room &room)
	:room_id{room.room_id}
	{}
};

struct ircd::m::room::state
:m::events
{
	id room_id;

	bool _query_(const event::where &, const event_closure_bool &) const override;
	bool _rquery_(const event::where &, const event_closure_bool &) const override;

	state(const id &room_id)
	:room_id{room_id}
	{}

	state(const room &room)
	:room_id{room.room_id}
	{}
};
