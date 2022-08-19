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

	using closure_bool = util::closure_bool
	<
		std::function,
		const m::room &, const string_view &
	>;

	m::user user;

  public:
	bool for_each(const string_view &membership, const closure_bool &) const;
	bool for_each(const closure_bool &) const;

	size_t count(const string_view &membership) const;
	size_t count() const;

	bool prefetch() const;

	rooms(const m::user &user)
	:user{user}
	{}
};
