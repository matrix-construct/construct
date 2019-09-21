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
#define HAVE_IRCD_PROF_H

namespace ircd::prof
{
	struct event;
	using group = std::vector<std::unique_ptr<event>>;
	IRCD_EXCEPTION(ircd::error, error)
	IRCD_OVERLOAD(sample)

	// Samples
	uint64_t cycles() noexcept;     ///< Monotonic reference cycles (since system boot)
	uint64_t time_user() noexcept;  ///< Nanoseconds of CPU time in userspace.
	uint64_t time_kern() noexcept;  ///< Nanoseconds of CPU time in kernelland.
	uint64_t time_real() noexcept;  ///< Nanoseconds of CPU time real.
	uint64_t time_proc();           ///< Nanoseconds of CPU time for process.
	uint64_t time_thrd();           ///< Nanoseconds of CPU time for thread.

	// Control panel
	void stop(group &);
	void start(group &);
	void reset(group &);
}

#include "x86.h"
#include "vg.h"
#include "type.h"
#include "scope_cycles.h"
#include "syscall_timer.h"
#include "instructions.h"
#include "resource.h"
#include "times.h"
#include "system.h"

// Exports to ircd::
namespace ircd
{
	using prof::cycles;
}

#if defined(__x86_64__) || defined(__i386__)
extern inline uint64_t
__attribute__((flatten, always_inline, gnu_inline, artificial))
ircd::prof::cycles()
noexcept
{
	return x86::rdtsc();
}
#else
ircd::prof::cycles()
noexcept
{
	static_assert(false, "Select reference cycle counter for platform.");
	return 0;
}
#endif
