// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2023 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_ALLOCATOR_PROFILE_H

namespace ircd::allocator
{
	struct profile;

	profile &operator+=(profile &, const profile &);
	profile &operator-=(profile &, const profile &);
	profile operator+(const profile &, const profile &);
	profile operator-(const profile &, const profile &);
}

/// Profiling counters. The purpose of this device is to gauge whether unwanted
/// or non-obvious allocations are taking place for a specific section. This
/// profiler has that very specific purpose and is not a replacement for
/// full-fledged memory profiling. This works by replacing global operator new
/// and delete, not any deeper hooks on malloc() at this time. To operate this
/// device you must first recompile and relink with RB_PROF_ALLOC defined at
/// least for the ircd/allocator.cc unit.
///
/// 1. Create an instance by copying the this_thread variable which will
/// snapshot the current counters.
/// 2. At some later point, create a second instance by copying the
/// this_thread variable again.
/// 3. Use the arithmetic operators to compute the difference between the two
/// snapshots and the result will be the count isolated between them.
struct ircd::allocator::profile
{
	uint64_t alloc_count {0};
	uint64_t free_count {0};
	size_t alloc_bytes {0};
	size_t free_bytes {0};

	/// Explicitly enabled by define at compile time only. Note: replaces
	/// global `new` and `delete` when enabled.
	static thread_local profile this_thread;
};
