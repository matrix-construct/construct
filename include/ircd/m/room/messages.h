// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2022 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_M_ROOM_MESSAGES_H

namespace ircd::m
{
	/// Returns `true` if `msg` reassigned with content from fetched `buf`.
	bool replace(room::message &msg, event::fetch &buf, const event::idx &);
}

/// Rendering
///
struct ircd::m::room::messages
{
	using closure = util::closure_bool
	<
		std::function, const message &, const uint64_t &, const event::idx &
	>;

	static const event::fetch::opts fopts;

	room::type events;
	bool redacted {false};

  public:
	bool for_each(const closure &) const;

	messages(const m::room &,
	         const decltype(room::type::range) &  = { -1UL, -1L },
	         const bool redacted = false);
};

inline
ircd::m::room::messages::messages(const m::room &room,
                                  const decltype(room::type::range) &range,
                                  const bool redacted)
:events
{
	room.room_id,
	"m.room.message",
	range,
	false,
}
,redacted
{
	redacted
}
{}
