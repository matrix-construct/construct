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
#define HAVE_IRCD_CTX_PROF_H

/// Profiling for the context system.
///
/// These facilities provide tools and statistics. The primary purpose here is
/// to alert developers of unwanted context behavior, in addition to optimizing
/// the overall performance of the context system.
///
/// The original use case is for the embedded database backend. Function calls
/// are made which may conduct blocking I/O before returning. This will hang the
/// current userspace context while it is running and thus BLOCK EVERY CONTEXT
/// in the entire IRCd. Since this is still an asynchronous system, it just
/// doesn't have callbacks: we do not do I/O without a cooperative yield.
/// Fortunately there are mechanisms to mitigate this -- but we have to know
/// for sure. A database call which has been passed over for mitigation may
/// start doing some blocking flush under load, etc. The profiler will alert
/// us of this so it doesn't silently degrade performance.
///
namespace ircd::ctx::prof
{
	enum class event :uint8_t;
	struct ticker;

	ulong cycles() noexcept;
	string_view reflect(const event &);

	// totals
	const ticker &get() noexcept;
	const uint64_t &get(const event &);

	// specific context
	const ticker &get(const ctx &c) noexcept;
	const uint64_t &get(const ctx &c, const event &);

	// current slice state
	ulong cur_slice_start() noexcept;
	ulong cur_slice_cycles() noexcept;

	// test accessors
	bool slice_exceeded_warning(const ulong &cycles) noexcept;
	bool slice_exceeded_assertion(const ulong &cycles) noexcept;
	bool slice_exceeded_interrupt(const ulong &cycles) noexcept;
	bool stack_exceeded_warning(const size_t &size) noexcept;
	bool stack_exceeded_assertion(const size_t &size) noexcept;

	extern log::log watchdog;
}

namespace ircd::ctx::prof::settings
{
	extern conf::item<double> stack_usage_warning;     // percentage
	extern conf::item<double> stack_usage_assertion;   // percentage

	extern conf::item<ulong> slice_warning;     // Warn when the yield-to-yield cycles exceeds
	extern conf::item<ulong> slice_interrupt;   // Interrupt exception when exceeded (not a signal)
	extern conf::item<ulong> slice_assertion;   // abort() when exceeded (not a signal, must yield)
}

/// Profiling events for marking. These are currently used internally at the
/// appropriate point to mark(): the user of ircd::ctx has no reason to mark()
/// these events; this interface is not quite developed for general use yet.
enum class ircd::ctx::prof::event
:uint8_t
{
	SPAWN,         // Context spawn requested
	JOIN,          // Context join requested
	JOINED,        // Context join completed
	ENTER,         // Current context entered
	LEAVE,         // Current context leaving
	YIELD,         // Current context yielding
	CONTINUE,      // Current context continuing
	INTERRUPT,     // Current context detects interruption
	TERMINATE,     // Current context detects termination
	CYCLES,        // monotonic counter (rdtsc)

	_NUM_
};

/// structure aggregating any profiling related state for a ctx
struct ircd::ctx::prof::ticker
{
	// monotonic counters for events
	std::array<uint64_t, num_of<prof::event>()> event {{0}};
};

/// Calculate the current reference cycle count (TSC) for the current
/// execution epoch/slice. This involves one RDTSC sample which is provided
/// by ircd::prof/ircd::prof::x86 (or for some other platform), and then
/// subtracting away the prior RDTSC sample which the context system makes
/// at the start of each execution slice.
extern inline ulong
__attribute__((flatten, always_inline, gnu_inline, artificial))
ircd::ctx::prof::cur_slice_cycles()
noexcept
{
	const auto stop(cycles());
	const auto start(cur_slice_start());
    return stop - start;
}

/// Sample the current TSC directly with an rdtsc; this is a convenience
/// wrapper leading to the ircd::prof interface which contains the platform
/// specific methods to efficiently sample a cycle count.
///
/// Developers are advised to obtain cycle counts from this_ctx::cycles()
/// which accumulates the cycle count for a specific ctx's execution only.
extern inline ulong
__attribute__((flatten, always_inline, gnu_inline, artificial))
ircd::ctx::prof::cycles()
noexcept
{
	return ircd::prof::cycles();
}
