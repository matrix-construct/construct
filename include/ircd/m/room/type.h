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
#define HAVE_IRCD_M_ROOM_TYPE_H

/// Interface to all room events sorted by type. This is not the "room type"
/// or m::type(room) classification string. This is an interface to the
/// _room_type table allowing efficient iteration of events similar to
/// room::events (_room_events) for a single event type. Events are sorted
/// by descending depth and event_idx within each type (similar to
/// room::events).
///
struct ircd::m::room::type
{
	using closure = std::function<bool (const string_view &, const uint64_t &, const event::idx &)>;

	m::room room;
	string_view _type;
	std::pair<uint64_t, int64_t> range; // highest (inclusive) to lowest (exclusive)
	bool prefixing; // true = startswith(type)

  public:
	bool for_each(const closure &) const;
	size_t count() const;
	bool empty() const;

	type(const m::room &,
	     const string_view &type  = {},
	     const decltype(range) &  = { -1UL, -1L },
	     const bool &prefixing    = false);

	static bool prefetch(const room::id &, const string_view &type, const int64_t &depth);
	static bool prefetch(const room::id &, const string_view &type);
};

inline
ircd::m::room::type::type(const m::room &room,
                          const string_view &type,
                          const decltype(range) &range,
                          const bool &prefixing)
:room{room}
,_type{type}
,range{range}
,prefixing{prefixing}
{}
