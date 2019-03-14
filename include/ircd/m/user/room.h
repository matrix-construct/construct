// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_M_USER_ROOM_H

struct ircd::m::user::room
:m::room
{
	m::user user;
	id::room::buf room_id;

	explicit
	room(const m::user &user,
	     const vm::copts *const & = nullptr,
	     const event::fetch::opts *const & = nullptr);

	room(const m::user::id &user_id,
	     const vm::copts *const & = nullptr,
	     const event::fetch::opts *const & = nullptr);

	room() = default;
	room(const room &) = delete;
	room &operator=(const room &) = delete;
};
