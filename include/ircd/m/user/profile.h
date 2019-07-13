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
#define HAVE_IRCD_M_USER_PROFILE_H

/// Interface to the user profile
struct ircd::m::user::profile
{
	using closure_bool = std::function<bool (const string_view &, const string_view &)>;
	using closure = std::function<void (const string_view &, const string_view &)>;

	m::user user;

	static void fetch(const m::user &, const net::hostport &, const string_view &key = {});

  public:
	bool for_each(const closure_bool &) const;
	bool get(std::nothrow_t, const string_view &key, const closure &) const;
	void get(const string_view &key, const closure &) const;
	string_view get(const mutable_buffer &out, const string_view &key) const; // nothrow
	event::id::buf set(const string_view &key, const string_view &val) const;

	profile(const m::user &user)
	:user{user}
	{}
};
