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
