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
#define HAVE_IRCD_M_USER_MITSEIN_H

/// Interface to the other users visible to a user from common rooms.
struct ircd::m::user::mitsein
{
	m::user user;

  public:
	// All common rooms with user
	bool for_each(const m::user &, const string_view &membership, const rooms::closure_bool &) const;
	void for_each(const m::user &, const string_view &membership, const rooms::closure &) const;
	bool for_each(const m::user &, const rooms::closure_bool &) const;
	void for_each(const m::user &, const rooms::closure &) const;

	// All common users in all rooms
	bool for_each(const string_view &membership, const closure_bool &) const;
	void for_each(const string_view &membership, const closure &) const;
	bool for_each(const closure_bool &) const;
	void for_each(const closure &) const;

	// Counting convenience
	size_t count(const m::user &, const string_view &membership = {}) const;
	size_t count(const string_view &membership = {}) const;

	// Existential convenience (does `user` and `other` share any common room).
	bool has(const m::user &other, const string_view &membership = {}) const;

	mitsein(const m::user &user);
};
