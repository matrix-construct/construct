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
#define HAVE_IRCD_M_USER_ROOMS_H

/// Interface to the rooms for a user.
struct ircd::m::user::rooms
{
	struct origins;

	using closure = std::function<void (const m::room &, const string_view &)>;
	using closure_bool = std::function<bool (const m::room &, const string_view &)>;

	m::user::room user_room;

  public:
	// All rooms with specific membership
	bool for_each(const string_view &membership, const closure_bool &) const;
	void for_each(const string_view &membership, const closure &) const;

	// All rooms with any membership
	bool for_each(const closure_bool &) const;
	void for_each(const closure &) const;

	size_t count(const string_view &membership) const;
	size_t count() const;

	rooms(const m::user &user);
};

/// Interface to the other servers visible to a user from all their rooms
struct ircd::m::user::rooms::origins
{
	using closure = m::room::origins::closure;
	using closure_bool = m::room::origins::closure_bool;

	m::user user;

  public:
	bool for_each(const string_view &membership, const closure_bool &) const;
	void for_each(const string_view &membership, const closure &) const;
	bool for_each(const closure_bool &) const;
	void for_each(const closure &) const;

	origins(const m::user &user);
};
