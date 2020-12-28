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
#define HAVE_IRCD_M_SEARCH_H

namespace ircd::m::search
{
	struct query;
	struct result;
	struct room_events;
}

struct ircd::m::search::room_events
:json::tuple
<
	/// Required. The string to search events for
	json::property<name::search_term, json::string>,

	/// The keys to search. Defaults to all. One of: ["content.body",
	/// "content.name", "content.topic"]
	json::property<name::keys, json::string>,

	/// This takes a filter
	json::property<name::filter, room_event_filter>,

	/// The order in which to search for results. By default, this is "rank".
	/// One of: ["recent", "rank"]
	json::property<name::order_by, json::string>,

	/// Configures whether any context for the events returned are included
	/// in the response.
	json::property<name::event_context, json::object>,

	/// Requests the server return the current state for each room returned.
	json::property<name::include_state, bool>,

	/// Requests that the server partitions the result set based on the
	/// provided list of keys.
	json::property<name::groupings, json::object>
>
{
	using super_type::tuple;
};

struct ircd::m::search::query
{
	user::id user_id;
	size_t batch {-1UL};
	search::room_events room_events;
	room_event_filter filter;
	string_view search_term;
	size_t limit {-1UL};
	ushort before_limit {0};
	ushort after_limit {0};
};

struct ircd::m::search::result
{
	json::stack *out {nullptr};
	util::timer elapsed;
	size_t skipped {0};
	size_t checked {0};
	size_t matched {0};
	size_t appends {0};
	size_t count {0};
	event::idx event_idx {0UL};
	long rank {0L};
};
