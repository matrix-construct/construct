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
#define HAVE_IRCD_M_SYNC_STATS_H

namespace ircd::m::sync
{
	struct stats;
}

/// Collection of statistics related items to not pollute the item/data
/// structures etc.
struct ircd::m::sync::stats
{
	static conf::item<bool> info;

	ircd::timer timer;
	size_t flush_bytes {0};
	size_t flush_count {0};
};
