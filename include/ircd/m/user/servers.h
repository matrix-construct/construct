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
#define HAVE_IRCD_M_USER_SERVERS_H

/// Interface to the other servers visible to a user from their rooms
struct ircd::m::user::servers
{
	using closure_bool = room::origins::closure_bool;

	m::user user;

  public:
	// All servers for the user in all their rooms
	bool for_each(const string_view &membership, const closure_bool &) const;
	bool for_each(const closure_bool &) const;

	// Counting convenience
	size_t count(const string_view &membership = {}) const;

	// Existential convenience (does `user` and `other` share any common room).
	bool has(const string_view &server, const string_view &membership = {}) const;

	servers(const m::user &user)
	:user{user}
	{}
};
