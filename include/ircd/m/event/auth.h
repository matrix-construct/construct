// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_M_EVENT_AUTH_H

namespace ircd::m
{
	bool is_power_event(const event &);
}

struct ircd::m::event::auth
{
	struct chain;

	event::idx idx;

  public:
	using closure_bool = event::closure_idx_bool;

	bool for_each(const string_view &type, const closure_bool &) const;
	bool for_each(const closure_bool &) const;

	bool has(const string_view &type) const noexcept;
	bool has(const event::idx &) const noexcept;

	size_t count(const string_view &type) const noexcept;
	size_t count() const noexcept;

	auth(const event::idx &idx)
	:idx{idx}
	{
		assert(idx);
	}
};

struct ircd::m::event::auth::chain
{
	event::auth auth;

  public:
	using closure = std::function<event::idx (const event::idx &)>;

	bool for_each(const string_view &type, const closure &) const;
	bool for_each(const closure &) const;

	chain(const event::idx &idx)
	:auth{idx}
	{}
};
