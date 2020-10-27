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
#define HAVE_IRCD_M_ROOM_CONTENT_H

/// Interfaced optimized for iterating the `content` of room events.
///
struct ircd::m::room::content
{
	using closure = std::function
	<
		bool (const json::object &, const uint64_t &, const event::idx &)
	>;

	static const size_t prefetch_max;
	static conf::item<size_t> prefetch;

	m::room room;
	std::pair<uint64_t, int64_t> range; // highest (inclusive) to lowest (exclusive)

  public:
	bool for_each(const closure &) const;

	content(const m::room &,
	        const decltype(range) &  = { -1UL, -1L });
};

inline
ircd::m::room::content::content(const m::room &room,
                                const decltype(range) &range)
:room{room}
,range{range}
{}
