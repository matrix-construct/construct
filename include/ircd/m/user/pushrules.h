// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_M_USER_PUSHRULES_H

struct ircd::m::user::pushrules
{
	using path = push::path;
	using closure_bool = std::function<bool (const event::idx &, const path &, const json::object &)>;
	using closure = std::function<void (const event::idx &, const path &, const json::object &)>;

	m::user user;

  public:
	bool for_each(const path &, const closure_bool &) const;
	bool for_each(const closure_bool &) const;
	bool get(std::nothrow_t, const path &, const closure &) const;
	void get(const path &, const closure &) const;
	bool set(const path &, const json::object &value) const;
	bool del(const path &) const;

	pushrules(const m::user &user) noexcept;
};

inline
ircd::m::user::pushrules::pushrules(const m::user &user)
noexcept
:user{user}
{}
