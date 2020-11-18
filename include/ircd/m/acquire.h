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
#define HAVE_IRCD_M_ACQUIRE_H

namespace ircd::m::acquire
{
	struct opts;
	struct execute;

	extern log::log log;
};

struct ircd::m::acquire::execute
{
	execute(const opts &);
};

struct ircd::m::acquire::opts
{
	/// Room apropos.
	m::room room;

	/// Optional remote host first considered as the target for operations in
	/// case caller has better information for what will most likely succeed.
	string_view hint;

	/// For diagnostic and special use; forces remote operations through the
	/// hint, and fails them if the hint is insufficient.
	bool hint_only {false};

	/// Perform head acquisition prior to depthwise operations.
	bool head {true};

	/// Perform missing acquisition.
	bool missing {true};

	/// Depthwise window of acquisition; concentrate on specific depth window
	pair<int64_t> depth {0, 0};

	/// Provide a viewport size; conf item
	size_t viewport_size {0};

	/// The number of rounds the algorithm runs for.
	size_t rounds {-1UL};

	/// Total event limit over all operations.
	size_t fetch_max {-1UL};

	/// Limit the number of requests in flight at any given time.
	size_t fetch_width {128};

	/// Avoids filling gaps with a depth sounding lte this value
	size_t gap_min {0};

	/// Avoids filling gaps with a depth sounding greater than this value
	size_t gap_max {-1UL};

	/// Won't fetch missing unless ref gt (newer) than this idx.
	event::idx ref_min {0};

	/// Won't fetch missing unless ref lt (older) than this idx.
	event::idx ref_max {-1UL};
};
