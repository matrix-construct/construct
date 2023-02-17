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
	struct sounding;
	struct missing;
	struct horizon;

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
	const m::event &fetch(std::nothrow_t, bool *valid = nullptr);
	const m::event &fetch();
	const m::event &operator*()        { return fetch(std::nothrow);           }
	const m::event *operator->()       { return &operator*();                  }

	// Prefetch the actual event data at the iterator's position (async)
	bool prefetch(const string_view &event_prop);
	bool prefetch(); // uses supplied fetch::opts.

	// Seeks to closest event in the room by depth; room.event_id ignored.
	events(const m::room &,
	       const uint64_t &depth,
	       const event::fetch::opts *const & = nullptr);

	// Seeks to event_id; null iteration when not found; seekless when empty.
	events(const m::room &,
	       const event::id &,
	       const event::fetch::opts *const & = nullptr);

	// Seeks to latest event in the room unless room.event_id given. Null
	// iteration when given and not found.
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
