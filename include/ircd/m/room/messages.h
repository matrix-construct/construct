// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_M_ROOM_MESSAGES_H

namespace ircd::m
{
	// [GET] Count the events in the room between two indexes.
	// Note the range here is unusual: The start index is exclusive, the ending
	// index is inclusive. The start index must be valid and in the room.
	size_t count_since(const room &, const m::event::idx &, const m::event::idx &);
	size_t count_since(const room &, const m::event::id &, const m::event::id &);
	size_t count_since(const m::event::idx &, const m::event::idx &);
	size_t count_since(const m::event::id &, const m::event::id &);

	// [GET] Find gaps in the room's event graph. A gap is where no events
	// have been obtained at that depth. Each gap is reported to the closure
	// with a separate invocation. The range is [inclusive, exclusive].
	using depth_range = std::pair<int64_t, int64_t>;
	using depth_range_closure = std::function<bool (const depth_range &, const event::idx &)>;
	bool for_each_depth_gap(const room &, const depth_range_closure &);
	bool rfor_each_depth_gap(const room &, const depth_range_closure &);

	std::pair<int64_t, event::idx> surface(const room &); // First missing
	std::pair<int64_t, event::idx> sounding(const room &); // Last missing
	std::pair<int64_t, event::idx> twain(const room &);
}

/// Interface to room messages
///
/// This interface has the form of an STL-style iterator over room messages
/// which are state and non-state events from all integrated timelines.
/// Moving the iterator is cheap, but the dereference operators fetch a
/// full event. One can iterate just event_idx's by using event_idx() instead
/// of the dereference operators.
///
struct ircd::m::room::messages
{
	m::room room;
	db::index::const_iterator it;
	event::fetch _event;

  public:
	operator bool() const              { return bool(it);                      }
	bool operator!() const             { return !it;                           }

	event::id::buf event_id() const;   // deprecated; will remove
	event::idx event_idx() const;      // Available from the iterator key.
	uint64_t depth() const;            // Available from the iterator key.
	string_view state_root() const;    // Available from the iterator value.

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

	messages(const m::room &room,
	         const uint64_t &depth,
	         const event::fetch::opts *const & = nullptr);

	messages(const m::room &room,
	         const event::id &,
	         const event::fetch::opts *const & = nullptr);

	messages(const m::room &room,
	         const event::fetch::opts *const & = nullptr);

	messages() = default;
	messages(const messages &) = delete;
	messages &operator=(const messages &) = delete;
};
