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
#define HAVE_IRCD_M_ROOM_EVENTS_SOUNDING_H

// The "viewport" is comprised of events starting from the tophead (most recent
// in room timeline) and covering about ~20 events leading up to that. Note
// that this is a completely ad hoc and configurable server value. Events in
// the viewport must be eval'ed and synced to clients in the order they will
// be displayed. Events not in the viewport are not /synced to clients and any
// client request provides event ordering: thus older events (backfills, etc)
// can be eval'ed without this constraint.
//
// The "sounding" is the depth of the first gap. In any attempt to trace
// the room timeline from the tophead to the m.room.create event: the sounding
// is the [highest number] depth preventing that.
//
// The "twain" marks the depth at the end of the first gap; the server is in
// possession of one or more events again at the twain.
//
// The "hazard" is the depth of the first gap starting from the m.room.create
// event toward the tophead. In any attempt to trace the room timeline with
// an increasing depth, the hazard is the next gap to frontfill.

namespace ircd::m
{
	std::pair<int64_t, event::idx> viewport(const room &);
	std::pair<int64_t, event::idx> sounding(const room &); // Last missing (one)
	std::pair<int64_t, event::idx> twain(const room &);
	std::pair<int64_t, event::idx> hazard(const room &); // First missing (one)
}

/// Find gaps in the room's events. A gap is where this server has no events
/// at a certain depth. This is a path-finding diagnostic interface, useful to
/// understand what areas of the timeline have not been acquired by the server
/// to calculate backfill requests, etc. This interface is depth-first oriented,
/// rather than the breadth-first room::missing interface.
///
struct ircd::m::room::events::sounding
{
	using range = std::pair<int64_t, int64_t>;
	using closure = util::function_bool
	<
		const range &, const event::idx &
	>;

	m::room room;

  public:
	bool for_each(const closure &) const;
	bool rfor_each(const closure &) const;

	sounding() = default;
	sounding(const m::room &room)
	:room{room}
	{}
};
