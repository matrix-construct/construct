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
	struct terminated {};                        // Special exception

	IRCD_OVERLOAD(threadsafe)

	extern const std::list<ctx *> &ctxs;         // List of all ctx instances

	const uint64_t &id(const ctx &);             // Unique ID for context
	string_view name(const ctx &);               // User's optional label for context
	const size_t &stack_max(const ctx &);        // Returns stack size allocated for ctx
	const size_t &stack_at(const ctx &);         // Stack at last sleep (also see this_ctx.h)
	const int32_t &notes(const ctx &);           // Peeks at internal semaphore count
	const uint64_t &yields(const ctx &);         // Context switching counter
	const ulong &cycles(const ctx &);            // Accumulated tsc (not counting cur slice)
	bool interruptible(const ctx &) noexcept;    // Context can throw at interruption point
	bool interruption(const ctx &) noexcept;     // Context was marked for interruption
	bool termination(const ctx &) noexcept;      // Context was marked for termination
	bool finished(const ctx &);                  // Context function returned (or exception).
	bool started(const ctx &);                   // Context was ever entered.
	bool running(const ctx &);                   // Context is the currently running ctx.
	bool waiting(const ctx &);                   // started() && !finished() && !running()

	void interruptible(ctx &, const bool &);     // False for interrupt suppression.
	void interrupt(ctx &);                       // Interrupt the context.
	void terminate(ctx &);                       // Interrupt for termination.
	void signal(ctx &, std::function<void ()>);  // Post function to context strand
	void notify(ctx &, threadsafe_t);            // Notify context with threadsafety.
	bool notify(ctx &);                          // Queue a context switch to arg
	void yield(ctx &);                           // Direct context switch to arg
}

#include "this_ctx.h"
#include "stack_usage_assertion.h"
#include "critical_assertion.h"
#include "critical_indicator.h"
#include "exception_handler.h"
#include "uninterruptible.h"
#include "prof.h"
#include "list.h"
#include "dock.h"
#include "latch.h"
#include "queue.h"
#include "mutex.h"
#include "shared_mutex.h"
#include "upgrade_lock.h"
#include "unlock_guard.h"
#include "condition_variable.h"
#include "view.h"
#include "shared_state.h"
#include "promise.h"
#include "future.h"
#include "when.h"
#include "context.h"
#include "async.h"
#include "pool.h"
#include "ole.h"
#include "fault.h"

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

	using ctx::prof::rdtsc;
}
