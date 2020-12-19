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
#define HAVE_IRCD_M_EVENT_INDEX_H

namespace ircd::m
{
	// Parallel query; returns number of successful results (positions are fixed)
	size_t index(const vector_view<event::idx> &out, const vector_view<const event::id> &in);

	// Parallel query; returns number number of ids. Successful results non-zero.
	size_t index(const vector_view<event::idx> &out, const m::event::prev &);
	size_t index(const vector_view<event::idx> &out, const m::event::auth &);

	// Responds with idx in closure; returns false if no action.
	bool index(std::nothrow_t, const event::id &, const event::closure_idx &);

	// Return idx from an event id
	[[nodiscard]] event::idx index(std::nothrow_t, const event::id &);
	event::idx index(const event::id &);

	// Return idx from populated event tuple
	[[nodiscard]] event::idx index(std::nothrow_t, const event &);
	event::idx index(const event &);
}
