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
#define HAVE_IRCD_M_EVENT_REFS_H

namespace ircd::m::dbs
{
	enum class ref :uint8_t;
}

/// Interface to the forward-references for an event. Forward-references are
/// virtually constructed from prev-references made by other events. This
/// interface queries the database which has pre-indexed the prev-references
/// made by other events at their insertion (it does not conduct any expensive
/// scan when using this interface, etc).
struct ircd::m::event::refs
{
	event::idx idx;

  public:
	using closure_bool = std::function<bool (const event::idx &, const dbs::ref &)>;

	bool for_each(const dbs::ref &type, const closure_bool &) const;
	bool for_each(const closure_bool &) const;

	bool has(const dbs::ref &type, const event::idx &) const noexcept;
	bool has(const event::idx &) const noexcept;

	size_t count(const dbs::ref &type) const noexcept;
	size_t count() const noexcept;

	refs(const event::idx &idx) noexcept;

	static void rebuild();
};

inline
ircd::m::event::refs::refs(const event::idx &idx)
noexcept
:idx{idx}
{
	assert(idx);
}
