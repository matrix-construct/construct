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

namespace ircd::m
{
	// [GET] Find gaps in the room's event graph. A gap is where no events
	// have been obtained at that depth. Each gap is reported to the closure
	// with a separate invocation. The range is [inclusive, exclusive].
	using depth_range = std::pair<int64_t, int64_t>;
	using depth_range_closure = std::function<bool (const depth_range &, const event::idx &)>;
	bool for_each_depth_gap(const room &, const depth_range_closure &);
	bool rfor_each_depth_gap(const room &, const depth_range_closure &);

	bool sounding(const room &, const depth_range_closure &); // Last missing (all)
	std::pair<int64_t, event::idx> hazard(const room &); // First missing (one)
	std::pair<int64_t, event::idx> sounding(const room &); // Last missing (one)
	std::pair<int64_t, event::idx> twain(const room &);
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
	m::room room;
	db::domain::const_iterator it;
	event::fetch _event;

  public:
	explicit operator bool() const     { return bool(it);                      }
	bool operator!() const             { return !it;                           }

	event::id::buf event_id() const;   // deprecated; will remove
	event::idx event_idx() const;      // Available from the iterator key.
	uint64_t depth() const;            // Available from the iterator key.

	explicit operator event::idx() const;

	const m::event &fetch(std::nothrow_t);
	const m::event &fetch();

	bool seek_idx(const event::idx &);
	bool seek(const uint64_t &depth = -1);
	bool seek(const event::id &);

	// These are reversed on purpose
	auto &operator++()                 { return --it;                          }
	auto &operator--()                 { return ++it;                          }

	const m::event &operator*();
	const m::event *operator->()       { return &operator*();                  }

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

	// [GET] Count the events in the room between two indexes.
	// Note the range here is unusual: The start index is exclusive, the ending
	// index is inclusive. The start index must be valid and in the room.
	static size_t count(const m::room &, const event::idx &, const event::idx &);
	static size_t count(const m::room &, const event::id &, const event::id &);
	static size_t count(const event::idx &, const event::idx &);
	static size_t count(const event::id &, const event::id &);
};
