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
#define HAVE_IRCD_M_EVENTS_H

namespace ircd::m::events
{
	struct range;
	using closure_bool = std::function<bool (const event::idx &, const event &)>;

	bool for_each(const range &, const closure_bool &);
	bool for_each(const range &, const event_filter &, const closure_bool &);
}

/// Range to start (inclusive) and stop (exclusive). If start is greater than
/// stop a reverse iteration will occur. -1 (or unsigned max value) can be used
/// to start or stop at the end. 0 can be used to start or stop at the beginning.
/// (event::idx of 0 is a sentinel)
///
struct ircd::m::events::range
:event::idx_range
{
	const event::fetch::opts *fopts {nullptr};

	range(const event::idx &start,
	      const event::idx &stop = -1,
	      const event::fetch::opts *const &fopts = nullptr)
	:event::idx_range{start, stop}
	,fopts{fopts}
	{}
};
