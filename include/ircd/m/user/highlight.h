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
#define HAVE_IRCD_M_USER_HIGHLIGHT_H

/// Interface to user highlighting and counting.
struct ircd::m::user::highlight
{
	m::user user;

	static conf::item<bool> enable_count;
	static conf::item<bool> match_mxid_full;
	static conf::item<bool> match_mxid_local_cs;
	static conf::item<bool> match_mxid_local_ci;

	bool match(const string_view &text) const;
	bool has(const event &) const;
	bool has(const event::idx &) const;

	size_t count_between(const m::room &, const event::idx_range &) const;
	size_t count_to(const m::room &, const event::idx &) const;
	size_t count(const m::room &) const;
	size_t count() const;

	highlight(const m::user &user)
	noexcept
	:user{user}
	{}
};
