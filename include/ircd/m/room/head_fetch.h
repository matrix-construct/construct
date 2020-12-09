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
#define HAVE_IRCD_M_ROOM_HEAD_FETCH_H

/// Fetch and determine the latest head information from all servers.
struct ircd::m::room::head::fetch
{
	struct opts;
	using closure = std::function<bool (const m::event &)>;

	static conf::item<milliseconds> timeout;

	/// Count of responding servers.
	size_t respond {0};

	/// Counts of servers reporting depth [behind, equal, ahead] relative to us.
	size_t depth[3] {0};

	/// Count of servers reporting origin_server_ts [behind, equal, ahead].
	size_t ots[3] {0};

	/// Total number of heads reported from all servers (including duplicates)
	size_t heads {0};

	/// Total number of concurrences for non-existent heads
	size_t concur {0};

	/// Total number of concurrences for existing heads.
	size_t exists {0};

	/// Running (and final) results when opts.unique=true; otherwise the
	/// closure is the only way to receive results.
	std::set<event::id::buf, std::less<>> head;

	// Primary operation; synchronous construction with results provided to
	// closure asynchronously.
	fetch(const opts &, const closure & = {});

	// Convenience operation
	static event one(const mutable_buffer &, const id &, const string_view &remote, const id::user & = {});
	static event::id::buf one(const id &, const string_view &remote, const id::user & = {});
};

struct ircd::m::room::head::fetch::opts
{
	/// Room apropos.
	m::id::room room_id;

	/// User for non-public rooms; note if not given one will be determined
	/// automatically.
	m::id::user user_id;

	/// Local reference frame; determined internally if not provided.
	std::tuple<m::id::event, int64_t, event::idx> top
	{
		m::id::event{}, 0L, 0UL
	};

	/// Limits total results
	size_t max_results {-1UL};

	/// Limits results per server (spec sez 20)
	size_t max_results_per_server {32};

	/// When true, results are stored in the head set and duplicate results
	/// are not provided to the closure. When false, the head set is not used.
	bool unique {true};

	/// When true, results may include events this server already has executed.
	bool existing {false};
};
