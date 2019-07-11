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
#define HAVE_IRCD_M_USER_USER_H

namespace ircd::m
{
	struct user;

	bool my(const user &);
	bool exists(const user &);
	bool exists(const id::user &);

	user create(const id::user &, const json::members &args = {});
}

/// This lightweight object is the strong type for a user.
///
/// Instances of this object are used as an argument in many places. The
/// sub-objects form special interfaces for the core tools and features
/// related to users. Not all user-related features are nested here,
/// only fundamentals which are generally used further by other features.
struct ircd::m::user
{
	struct room;
	struct rooms;
	struct mitsein;
	struct events;
	struct profile;
	struct account_data;
	struct room_account_data;
	struct room_tags;
	struct filter;
	struct ignores;
	struct highlight;

	using id = m::id::user;
	using closure = std::function<void (const user &)>;
	using closure_bool = std::function<bool (const user &)>;

	static m::room users;
	static m::room tokens;

	id user_id;

	operator const id &() const;

	static string_view gen_access_token(const mutable_buffer &out);
	static id::device::buf get_device_from_access_token(const string_view &token);

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

inline ircd::m::user::operator
const ircd::m::user::id &()
const
{
	return user_id;
}

#include "room.h"
#include "rooms.h"
#include "mitsein.h"
#include "events.h"
#include "profile.h"
#include "account_data.h"
#include "room_account_data.h"
#include "room_tags.h"
#include "filter.h"
#include "ignores.h"
#include "highlight.h"
