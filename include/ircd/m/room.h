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

	struct room; // see: room/room.h

	// Util
	bool my(const room &);

	// [GET] Util
	bool exists(const room &);
	bool exists(const id::room &);
	bool exists(const id::room_alias &, const bool &remote = false);
	uint version(const id::room &);
	bool federate(const id::room &);
	id::user::buf creator(const id::room &);
	bool creator(const id::room &, const id::user &);

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
	event::id::buf invite(const room &, const m::id::user &target, const m::id::user &sender);
	event::id::buf leave(const room &, const m::id::user &);
	event::id::buf join(const room &, const m::id::user &);
	event::id::buf join(const id::room_alias &, const m::id::user &);

	// [SET] Create new room
	room create(const id::room &, const id::user &creator, const id::room &parent, const string_view &type);
	room create(const id::room &, const id::user &creator, const string_view &type = {});
}

#include "room/room.h"
#include "room/messages.h"
#include "room/state.h"
#include "room/members.h"
#include "room/origins.h"
#include "room/head.h"
#include "room/power.h"
