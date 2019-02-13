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

struct ircd::m::event::auth
{
	struct refs;
	struct chain;

	static bool is_power_event(const event &);
	static string_view failed(const event &, const vector_view<const event> &auth_events);
	static string_view failed(const event &);
	static bool check(std::nothrow_t, const event &);
	static void check(const event &);
};

/// Interface to the references made by other power events to this power
/// event in the `auth_events`. This interface only deals with power events,
/// it doesn't care if a non-power event referenced a power event. This does
/// not contain the auth-chain or state resolution algorithm here, those are
/// later constructed out of this data.
struct ircd::m::event::auth::refs
{
	event::idx idx;

  public:
	using closure_bool = event::closure_idx_bool;

	bool for_each(const string_view &type, const closure_bool &) const;
	bool for_each(const closure_bool &) const;

	bool has(const string_view &type) const noexcept;
	bool has(const event::idx &) const noexcept;

	size_t count(const string_view &type) const noexcept;
	size_t count() const noexcept;

	refs(const event::idx &idx)
	:idx{idx}
	{
		assert(idx);
	}

	static void rebuild();
};

struct ircd::m::event::auth::chain
{
	event::idx idx;

  public:
	using closure_bool = std::function<bool (const vector_view<const event::id> &)>;

	bool for_each(const closure_bool &) const;
	bool has(const string_view &type) const noexcept;
	size_t depth() const noexcept;

	chain(const event::idx &idx)
	:idx{idx}
	{
		assert(idx);
	}
};
