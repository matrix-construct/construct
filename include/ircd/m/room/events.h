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
	using depth_range = std::pair<uint64_t, uint64_t>;
	using entry = std::tuple<uint64_t, event::idx>;

	static conf::item<ssize_t> viewport_size;

	m::room room;
	db::domain::const_iterator it;
	mutable entry _entry;

  public:
	explicit operator bool() const;
	bool operator!() const;

	// Fetch the actual event data at the iterator's position
	const entry *operator->() const;
	const entry &operator*() const;

	// Observers from the current valid iterator
	event::idx event_idx() const;
	uint64_t depth() const;

	// Move the iterator
	auto &operator++();
	auto &operator--();

	// Perform a new lookup / iterator
	bool seek_idx(const event::idx &, const bool &lower_bound = false);
	bool seek(const uint64_t &depth = -1);
	bool seek(const event::id &);

	// Prefetch a new iterator lookup (async)
	bool preseek(const uint64_t &depth = -1);

	// Prefetch the actual event data at the iterator's position (async)
	bool prefetch(const string_view &event_prop);
	bool prefetch(); // uses supplied fetch::opts.

	// Seeks to closest event in the room by depth; room.event_id ignored.
	events(const m::room &,
	       const uint64_t &depth,
	       const event::fetch::opts * = nullptr);

	// Seeks to event_id; null iteration when not found; seekless when empty.
	events(const m::room &,
	       const event::id &,
	       const event::fetch::opts * = nullptr);

	// Seeks to latest event in the room unless room.event_id given. Null
	// iteration when given and not found.
	events(const m::room &,
	       const event::fetch::opts * = nullptr);

	events() = default;
	events(const events &) = delete;
	events &operator=(const events &) = delete;

	// Prefetch a new iterator (without any construction)
	static bool preseek(const m::room &, const uint64_t &depth = -1);

	// Prefetch the actual room event data for a range; or most recent.
	static size_t prefetch(const m::room &, const depth_range &);
	static size_t prefetch_viewport(const m::room &);

	// Note the range here is unusual: The start index is exclusive, the ending
	// index is inclusive. The start index must be valid and in the room.
	static size_t count(const m::room &, const event::idx_range &);
	static size_t count(const event::idx_range &);
};

inline auto &
ircd::m::room::events::operator++()
{
	return --it;
}

inline auto &
ircd::m::room::events::operator--()
{
	return ++it;
}

inline uint64_t
ircd::m::room::events::depth()
const
{
	return std::get<0>(operator*());
}

inline ircd::m::event::idx
ircd::m::room::events::event_idx()
const
{
	return std::get<1>(operator*());
}

inline
const ircd::m::room::events::entry &
ircd::m::room::events::operator*()
const
{
	return *(operator->());
}

inline bool
ircd::m::room::events::operator!()
const
{
	return !it;
}

inline ircd::m::room::events::operator
bool()
const
{
	return bool(it);
}
