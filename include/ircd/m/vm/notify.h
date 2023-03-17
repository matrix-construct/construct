// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2023 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_M_VM_NOTIFY_H

namespace ircd::m::vm::notify
{
	using value_type = std::pair<const event::id, ctx::promise<> *>;
	using alloc_type = allocator::node<value_type>;
	using map_type = std::multimap<const event::id, ctx::promise<> *, std::less<>, alloc_type::allocator>;
	using node_type = std::pair<map_type::node_type, value_type>;

	extern map_type map;

	size_t wait(const vector_view<const event::id> &, const milliseconds);
	bool wait(const event::id &, const milliseconds);
}

/// Yields ctx until event was successfully evaluated. Returns false if
/// timeout occurred.
inline bool
ircd::m::vm::notify::wait(const event::id &event_id,
                          const milliseconds to)
{
	const vector_view<const event::id> vector
	(
		&event_id, 1
	);

	const auto count
	{
		wait(vector, to)
	};

	return count;
}
