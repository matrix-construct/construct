// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2023 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_M_ROOM_MISSING_H

/// Find missing room events. This is a breadth-first iteration of missing
/// references from the tophead (or at the event provided in the room arg)
///
/// The closure is invoked with the first argument being the event_id unknown
/// to the server, followed by the depth and event::idx of the event making the
/// reference.
///
struct ircd::m::room::missing
{
	using closure = util::function_bool
	<
		const event::id &, const uint64_t &, const event::idx &
	>;

	m::room room;

  private:
	bool _each(m::room::events &, m::event::fetch &, const closure &) const;

  public:
	bool rfor_each(const pair<int64_t> &depth, const closure &) const;
	bool for_each(const pair<int64_t> &depth, const closure &) const;
	bool for_each(const closure &) const;
	size_t count() const;

	missing() = default;
	missing(const m::room &room)
	:room{room}
	{}
};
