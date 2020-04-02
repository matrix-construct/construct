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
#define HAVE_IRCD_M_USER_TOKENS_H

struct ircd::m::user::tokens
{
	using closure = std::function<void (const event::idx &, const string_view &)>;
	using closure_bool = std::function<bool (const event::idx &, const string_view &)>;

	static string_view generate(const mutable_buffer &out);
	static id::device::buf device(std::nothrow_t, const string_view &token);
	static id::device::buf device(const string_view &token);
	static id::user::buf get(std::nothrow_t, const string_view &token);
	static id::user::buf get(const string_view &token);

	m::user user;

	bool for_each(const closure_bool &) const;
	bool check(const string_view &token) const;
	bool del(const string_view &token, const string_view &reason) const;
	size_t del(const string_view &reason) const;

	tokens(const m::user &user)
	:user{user}
	{}
};
