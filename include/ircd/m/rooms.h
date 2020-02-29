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
#define HAVE_IRCD_M_ROOMS_H

/// Convenience iterface for rooms; utilities for the server's collection of
/// rooms and some rooms list related linkages.
///
namespace ircd::m::rooms
{
	struct opts extern const opts_default;

	// Iterate the rooms
	bool for_each(const opts &, const room::id::closure_bool &);
	bool for_each(const room::id::closure_bool &);

	// tools
	size_t count(const opts & = opts_default);
	bool has(const opts & = opts_default);

	// util
	void dump__file(const opts &, const string_view &filename);
}

/// Arguments structure to rooms::for_each(). This reduces the API surface to
/// handle a rich set of ways to iterate over the rooms.
struct ircd::m::rooms::opts
{
	/// A full or partial room_id can be defined; partial is only valid if
	/// lower_bound is true.
	string_view room_id;

	/// Set a string for the join_rule; undefined matches all. For example,
	/// if set to "join" then the iteration can list public rooms.
	string_view join_rule;

	/// Set a string to localize query to a single server
	string_view server;

	/// Room alias prefix search
	string_view room_alias;

	/// user::rooms convenience
	id::user user_id;

	/// Filters results to those that have a public rooms list summary
	bool summary {false};

	/// Indicates if the interface treats the room_id specified as a lower
	/// bound rather than exact match. This means an iteration will start
	/// at the same or next key, and continue indefinitely. By false default,
	/// when a room_id is given a for_each() will have 0 or 1 iterations.
	bool lower_bound {false};

	/// If set to true, results are limited to rooms where no other server
	/// is a member of the room. No memberships even those in the leave state
	/// originated from another server.
	bool local_only {false};

	/// If set to true, the results are filtered to those rooms which have a
	/// member from anoher server. Note that member may be in the leave state.
	bool remote_only {false};

	/// If set to true, rooms which have no members from this server presently
	/// in the join state are filtered from the results.
	bool local_joined_only {false};

	/// If set to true, rooms where no other server has a presently joined user
	/// are filtered from the results.
	bool remote_joined_only {false};

	/// Spec search term
	string_view search_term;

	/// Specify prefetching to increase iteration performance.
	size_t prefetch {0};

	opts() = default;
	opts(const string_view &search_term) noexcept; // special
};
