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
#define HAVE_IRCD_M_ROOM_EVENTS_HORIZON_H

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
