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
#define HAVE_IRCD_M_USER_H

namespace ircd::m
{
	struct user;

	bool my(const user &);
	bool exists(const user &);
	bool exists(const id::user &);
	user create(const id::user &, const json::members &args = {});
}

struct ircd::m::user
{
	struct room;
	using id = m::id::user;

	id user_id;

	operator const id &() const;

	static m::room users;
	static m::room tokens;

	static string_view gen_access_token(const mutable_buffer &out);

	id::room room_id(const mutable_buffer &) const;
	id::room::buf room_id() const;

	bool is_password(const string_view &password) const noexcept;
	event::id::buf password(const string_view &password);

	bool is_active() const;
	event::id::buf deactivate();
	event::id::buf activate();

	user(const id &user_id)
	:user_id{user_id}
	{}

	user() = default;
};

struct ircd::m::user::room
:m::room
{
	m::user user;
	id::room::buf room_id;

	room(const m::user &user);
	room(const m::user::id &user_id);
	room() = default;
	room(const room &) = delete;
	room &operator=(const room &) = delete;
};

inline ircd::m::user::operator
const ircd::m::user::id &()
const
{
	return user_id;
}
