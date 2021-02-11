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
#define HAVE_IRCD_M_ROOM_EVENTS_H

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

/// Interface to room events
///
/// This interface has the form of an STL-style iterator over room events
/// which are state and non-state events from all integrated timelines.
/// Moving the iterator is cheap, but the dereference operators fetch a
/// full event. One can iterate just event_idx's by using event_idx() instead
/// of the dereference operators.
///
struct ircd::m::room::events
{
	struct sounding;
	struct horizon;
	struct missing;

	static conf::item<ssize_t> viewport_size;

	m::room room;
	db::domain::const_iterator it;
	event::fetch _event;

  public:
	explicit operator bool() const     { return bool(it);                      }
	bool operator!() const             { return !it;                           }

	// Observers from the current valid iterator
	uint64_t depth() const;
	event::idx event_idx() const;
	explicit operator event::idx() const;

	// Perform a new lookup / iterator
	bool seek_idx(const event::idx &, const bool &lower_bound = false);
	bool seek(const uint64_t &depth = -1);
	bool seek(const event::id &);

	// Prefetch a new iterator lookup (async)
	bool preseek(const uint64_t &depth = -1);

	// Move the iterator
	auto &operator++()                 { return --it;                          }
	auto &operator--()                 { return ++it;                          }

	// Fetch the actual event data at the iterator's position
	const m::event &operator*();
	const m::event *operator->()       { return &operator*();                  }
	const m::event &fetch(std::nothrow_t);
	const m::event &fetch();

	// Prefetch the actual event data at the iterator's position (async)
	bool prefetch(const string_view &event_prop);
	bool prefetch(); // uses supplied fetch::opts.

	events(const m::room &,
	       const uint64_t &depth,
	       const event::fetch::opts *const & = nullptr);

	events(const m::room &,
	       const event::id &,
	       const event::fetch::opts *const & = nullptr);

	events(const m::room &,
	       const event::fetch::opts *const & = nullptr);

	events() = default;
	events(const events &) = delete;
	events &operator=(const events &) = delete;

	// Prefetch a new iterator (without any construction)
	static bool preseek(const m::room &, const uint64_t &depth = -1);

	// Prefetch the actual room event data for a range; or most recent.
	using depth_range = std::pair<uint64_t, uint64_t>;
	static size_t prefetch(const m::room &, const depth_range &);
	static size_t prefetch_viewport(const m::room &);

	// Note the range here is unusual: The start index is exclusive, the ending
	// index is inclusive. The start index must be valid and in the room.
	static size_t count(const m::room &, const event::idx_range &);
	static size_t count(const event::idx_range &);
};

/// Find missing room events. This is a breadth-first iteration of missing
/// references from the tophead (or at the event provided in the room arg)
///
/// The closure is invoked with the first argument being the event_id unknown
/// to the server, followed by the depth and event::idx of the event making the
/// reference.
///
struct ircd::m::room::events::missing
{
	using closure = std::function<bool (const event::id &, const uint64_t &, const event::idx &)>;

	m::room room;

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

/// Find gaps in the room's events. A gap is where this server has no events
/// at a certain depth. This is a path-finding diagnostic interface, useful to
/// understand what areas of the timeline have not been acquired by the server
/// to calculate backfill requests, etc. This interface is depth-first oriented,
/// rather than the breadth-first room::events::missing interface.
///
struct ircd::m::room::events::sounding
{
	using range = std::pair<int64_t, int64_t>;
	using closure = std::function<bool (const range &, const event::idx &)>;

	m::room room;

  public:
	bool for_each(const closure &) const;
	bool rfor_each(const closure &) const;

	sounding() = default;
	sounding(const m::room &room)
	:room{room}
	{}
};

/// Find missing room events. This is an interface to the event-horizon for
/// this room. The event horizon is keyed by event_id and the value is the
/// event::idx of the event referencing it. There can be multiple entries for
/// an event_id. The closure is also invoked with the depth of the referencer.
///
struct ircd::m::room::events::horizon
{
	using closure = std::function<bool (const event::id &, const uint64_t &, const event::idx &)>;

	m::room room;

  public:
	bool for_each(const closure &) const;
	size_t count() const;

	size_t rebuild();

	horizon() = default;
	horizon(const m::room &room)
	:room{room}
	{}
};
