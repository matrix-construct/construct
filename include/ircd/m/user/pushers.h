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
#define HAVE_IRCD_M_USER_PUSHERS_H

struct ircd::m::user::pushers
{
	using closure_bool = std::function<bool (const event::idx &, const string_view &, const json::object &)>;
	using closure = std::function<void (const event::idx &, const string_view &, const json::object &)>;

	m::user user;

  public:

	bool for_each(const closure_bool &) const;

	size_t count(const string_view &kind = {}) const;
	bool any(const string_view &kind = {}) const;
	bool has(const string_view &key) const;

	bool get(std::nothrow_t, const string_view &key, const closure &) const;
	void get(const string_view &key, const closure &) const;
	bool set(const json::object &value) const;
	bool del(const string_view &key) const;

	pushers(const m::user &user) noexcept;
};

inline
ircd::m::user::pushers::pushers(const m::user &user)
noexcept
:user{user}
{}
