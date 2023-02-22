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
#define HAVE_IRCD_M_ROOM_ITERATE_H

/// Interfaced optimized for pipelined iterations of room events.
///
struct ircd::m::room::iterate
{
	using entry = pair<uint64_t, m::event::idx>;
	using closure = util::function_bool
	<
		const string_view, const uint64_t, const event::idx
	>;

	static conf::item<size_t> prefetch;

	m::room room;
	string_view column;
	std::pair<uint64_t, int64_t> range; // highest (inclusive) to lowest (exclusive)
	size_t queue_max;
	std::unique_ptr<entry[]> buf;

  public:
	bool for_each(const closure &) const;

	iterate(const m::room &,
	        const string_view &column  = {},
	        const decltype(range) &    = { -1UL, -1L });

	iterate(const iterate &) = delete;
	iterate(iterate &&) = delete;
};

inline
ircd::m::room::iterate::iterate(const m::room &room,
                                const string_view &column,
                                const decltype(range) &range)
:room{room}
,column{column}
,range{range}
,queue_max{prefetch}
,buf
{
	new entry[queue_max]
	{
		{ 0UL, 0UL }
	}
}
{}
