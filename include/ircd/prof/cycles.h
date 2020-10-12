// The Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_PROF_CYCLES_H

namespace ircd::prof
{
	uint64_t cycles() noexcept;
}

namespace ircd
{
	using prof::cycles;
}

/// Monotonic reference cycles (since system boot)
extern inline uint64_t
__attribute__((flatten, always_inline, gnu_inline, artificial))
ircd::prof::cycles()
noexcept
{
	#if defined(__x86_64__) || defined(__i386__)
		return x86::rdtsc();
	#elif defined(__aarch64__)
		return arm::read_virtual_counter();
	#elif __has_builtin(__builtin_readcyclecounter)
		return __builtin_readcyclecounter();
	#else
		static_assert(false, "Select reference cycle counter for platform.");
		return 0;
	#endif
}
