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
}

/// Interface to tools that build a publicrooms summary from room state.
namespace ircd::m::rooms::summary
{
	struct fetch;

	// observers
	bool has(const room::id &);
	void chunk(const room &, json::stack::object &chunk);
	json::object chunk(const room &, const mutable_buffer &out);

	// mutators
	event::id::buf set(const room::id &, const json::object &summary);
	event::id::buf set(const room &);
	event::id::buf del(const room &);
}

struct ircd::m::rooms::summary::fetch
{
	// conf
	static conf::item<size_t> limit;
	static conf::item<seconds> timeout;

	// result
	size_t total_room_count_estimate {0};
	std::string next_batch;

	// request
	fetch(const net::hostport &hp,
	      const string_view &since   =  {},
	      const size_t &limit        = 64);

	fetch() = default;
};

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

	/// Spec search term
	string_view search_term;

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

	/// Local-only (see room/room.h). If set to true, results are limited to
	/// rooms where no other server is joined to the room.
	bool local_only {false};

	/// Non-lonly (see room/room.h). If set to true, rooms where no
	/// other server is joined to the room are filtered from the results.
	bool remote_joined_only {false};
};
