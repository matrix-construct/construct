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
#define HAVE_IRCD_M_ROOM_STATE_FETCH_H

/// Fetch and determine the latest head information from all servers.
struct ircd::m::room::state::fetch
{
	struct opts;
	using closure = std::function<bool (const m::event::id &, const string_view &)>;

	static conf::item<milliseconds> timeout;

	/// Count of responding servers.
	size_t respond {0};

	/// Total number of states reported from all servers (including duplicates)
	size_t responses {0};

	/// Total number of concurrences for non-existent states.
	size_t concur {0};

	/// Total number of concurrences for existing states.
	size_t exists {0};

	/// Running (and final) results when opts.unique=true; otherwise the
	/// closure is the only way to receive results.
	std::set<event::id::buf, std::less<>> result;

	// Primary operation; synchronous construction with results provided to
	// closure asynchronously.
	fetch(const opts &, const closure & = {});
};

struct ircd::m::room::state::fetch::opts
{
	/// Room apropos.
	m::room room;

	/// When true, results are stored in the result set and duplicate results
	/// are not provided to the closure. When false, the result set is not used.
	bool unique {true};

	/// When true, results may include events this server already has executed.
	bool existing {false};
};
