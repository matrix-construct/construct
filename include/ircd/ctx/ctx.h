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
#define HAVE_IRCD_CTX_H

/// Userspace Contexts: cooperative threading from stackful coroutines.
///
/// This is the public interface to the userspace context system. No 3rd party
/// symbols are included from here. This file is included automatically in stdinc.h
/// and you do not have to include it manually.
///
/// There are two primary objects at work in the context system:
///
/// `struct context` <ircd/ctx/context.h>
/// Public interface emulating std::thread; included automatically from here.
/// To spawn and manipulate contexts, deal with this object.
///
/// `struct ctx` (ircd/ctx.cc)
/// Internal implementation of the context. This is not included here.
/// Several low-level functions are exposed for library creators. This file is usually
/// included when boost/asio.hpp is also included and calls are actually made into boost.
///
/// boost::asio is not included from here. To access that include boost in a
/// definition file with #include <ircd/asio.h>. That include contains some
/// devices we use to yield a context to asio.
///
namespace ircd::ctx
{
	struct ctx;

	IRCD_EXCEPTION(ircd::error, error)
	IRCD_EXCEPTION(error, interrupted)
	IRCD_EXCEPTION(error, timeout)
	struct terminated {};                              // Special exception

	IRCD_OVERLOAD(threadsafe)
	bool is_main_thread() noexcept;
	void assert_main_thread();

	const uint64_t &id(const ctx &) noexcept;          // Unique ID for context
	string_view name(const ctx &) noexcept;            // User's optional label for context
	const uint32_t &flags(const ctx &) noexcept;       // Direct flags access
	const int32_t &notes(const ctx &) noexcept;        // Peeks at internal semaphore count
	const uint64_t &epoch(const ctx &) noexcept;       // Context switching counter
	const ulong &cycles(const ctx &) noexcept;         // Accumulated tsc (not counting cur slice)
	const int8_t &ionice(const ctx &) noexcept;        // IO priority nice-value
	const int8_t &nice(const ctx &) noexcept;          // Scheduling priority nice-value
	bool interruptible(const ctx &) noexcept;          // Context can throw at interruption point
	bool interruption(const ctx &) noexcept;           // Context was marked for interruption
	bool termination(const ctx &) noexcept;            // Context was marked for termination
	bool finished(const ctx &) noexcept;               // Context function returned (or exception).
	bool started(const ctx &) noexcept;                // Context was ever entered.
	bool running(const ctx &) noexcept;                // Context is the currently running ctx.
	bool waiting(const ctx &) noexcept;                // started() && !finished() && !running()
	bool queued(const ctx &) noexcept;                 // !running() && notes() > 0

	uint32_t &flags(ctx &) noexcept;                   // Direct flags access
	int8_t ionice(ctx &, const int8_t &) noexcept;     // IO priority nice-value
	int8_t nice(ctx &, const int8_t &) noexcept;       // Scheduling priority nice-value
	void name(ctx &, const string_view &) noexcept;    // Change the name (truncates to 15 chars)
	void interruptible(ctx &, const bool &) noexcept;  // False for interrupt suppression.
	void interrupt(ctx &);                             // Interrupt the context.
	void terminate(ctx &);                             // Interrupt for termination.
	void signal(ctx &, std::function<void ()>);        // Post function to context strand
	void notify(ctx &, threadsafe_t);                  // Notify context with threadsafety.
	bool notify(ctx &) noexcept;                       // Queue a context switch to arg
	void yield(ctx &);                                 // Direct context switch to arg

	bool for_each(const std::function<bool (ctx &)> &);
	const uint64_t &epoch() noexcept;

	extern log::log log;
}

#include "prof.h"
#include "this_ctx.h"
#include "context.h"
#include "wait.h"
#include "sleep.h"
#include "stack.h"
#include "stack_usage_assertion.h"
#include "slice_usage_warning.h"
#include "critical_assertion.h"
#include "critical_indicator.h"
#include "exception_handler.h"
#include "uninterruptible.h"
#include "list.h"
#include "dock.h"
#include "latch.h"
#include "queue.h"
#include "shared_mutex.h"
#include "upgrade_lock.h"
#include "unlock_guard.h"
#include "condition_variable.h"
#include "scope_notify.h"
#include "mutex.h"
#include "view.h"
#include "shared_state.h"
#include "promise.h"
#include "future.h"
#include "when.h"
#include "async.h"
#include "pool.h"
#include "ole.h"
#include "fault.h"
#include "concurrent.h"
#include "concurrent_for_each.h"
#include "trit.h"
#include "posix.h"

// Exports to ircd::
namespace ircd
{
	//using yield = boost::asio::yield_context;
	namespace this_ctx = ctx::this_ctx;

	using ctx::timeout;
	using ctx::context;
	using ctx::sleep;

	using ctx::promise;
	using ctx::future;

	using ctx::use_future_t;
	using ctx::use_future;

	using ctx::critical_assertion;
	using ctx::critical_indicator;

	using ctx::is_main_thread;
	using ctx::assert_main_thread;
}

/// Marks `ctx` for whether to allow or suppress interruption. Suppression
/// does not ignore an interrupt itself, it only ignores the interruption
/// points. Thus when a suppression ends if the interrupt flag was ever set
/// the next interruption point will throw as expected.
inline void
ircd::ctx::interruptible(ctx &ctx,
                         const bool &b)
noexcept
{
	flags(ctx) ^= (flags(ctx) ^ (ulong(b) - 1)) & context::NOINTERRUPT;
	assert(bool(flags(ctx) & context::NOINTERRUPT) == !b);
	assert(interruptible(ctx) == b);
}

/// Indicates if `ctx` was terminated; does not clear the flag
inline bool
ircd::ctx::termination(const ctx &c)
noexcept
{
	return flags(c) & context::TERMINATED;
}

/// Indicates if `ctx` was interrupted; does not clear the flag
inline bool
ircd::ctx::interruption(const ctx &c)
noexcept
{
	return flags(c) & context::INTERRUPTED;
}

inline bool
ircd::ctx::interruptible(const ctx &ctx)
noexcept
{
	return ~flags(ctx) & context::NOINTERRUPT;
}

/// Returns the cycle count for `ctx`
inline const ulong &
ircd::ctx::cycles(const ctx &ctx)
noexcept
{
	return prof::get(ctx, prof::event::CYCLES);
}

/// Returns the yield count for `ctx`
inline const uint64_t &
ircd::ctx::epoch(const ctx &ctx)
noexcept
{
	return prof::get(ctx, prof::event::YIELD);
}

inline void
__attribute__((always_inline))
ircd::ctx::assert_main_thread()
{
	assert(is_main_thread());
}

inline bool
__attribute__((always_inline))
ircd::ctx::is_main_thread()
noexcept
{
	return ios::is_main_thread;
}
