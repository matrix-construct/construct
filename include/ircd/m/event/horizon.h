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
#define HAVE_IRCD_M_EVENT_HORIZON_H

/// Interface to the set of unresolved events. This set contains event_id's
/// which the server does not have. For each event_id in the set, there is an
/// event::idx of an event which made a reference to the event_id. There may be
/// multiple entries for an event_id.
///
/// This information helps construct the dependency graph in event::refs out of
/// order; it is anti-reference which is removed after the event is processed.
/// Each event::idx for an event_id in the set is then able to be "reprocessed"
/// in some way; at a minimum the reprocessing removes the event_id|event::idx
/// entry (see: dbs/event_horizon related).
///
class ircd::m::event::horizon
{
	event::id event_id;

  public:
	using closure_bool = std::function<bool (const event::id &, const event::idx &)>;

	bool for_each(const closure_bool &) const;
	bool has(const event::idx &) const;
	size_t count() const;

	horizon(const event::id &);
	horizon() = default;

	static bool has(const event::id &);
	static size_t rebuild();
};

inline
ircd::m::event::horizon::horizon(const event::id &event_id)
:event_id{event_id}
{
	assert(event_id);
}
