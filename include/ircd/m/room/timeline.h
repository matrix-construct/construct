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
#define HAVE_IRCD_M_ROOM_TIMELINE_H

namespace ircd::m
{
	uint64_t latency(const room::timeline &, const room::timeline &);
}

/// Interface to room timeline
///
struct ircd::m::room::timeline
{
	struct coord;
	using closure = std::function<bool (const coord &, const event::idx &)>;

	m::room room;

  public:
	bool has_past(const event::id &) const;
	bool has_future(const event::id &) const;

	bool for_each(const closure &, const coord &branch) const;

	timeline(const m::room &);
	timeline() = default;
	timeline(const timeline &) = delete;
	timeline &operator=(const timeline &) = delete;

	static void rebuild(const m::room &);
};

struct ircd::m::room::timeline::coord
{
	int64_t x {0};
	int64_t y {0};
};
