/*
 * charybdis: oh just a little chat server
 * ctx.h: userland context switching (stackful coroutines)
 *
 * Copyright (C) 2016 Charybdis Development Team
 * Copyright (C) 2016 Jason Volk <jason@zemos.net>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice is present in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once
#define HAVE_IRCD_CTX_H

//
// This is the public interface to the userspace context system. No 3rd party
// symbols are included from here. This file is included automatically in stdinc.h
// and you do not have to include it manually.
//
// There are two primary objects at work in the context system:
//
// `struct context` <ircd/ctx/context.h>
// Public interface emulating std::thread; included automatically from here.
// To spawn and manipulate contexts, deal with this object.
//
// `struct ctx` <ircd/ctx/ctx.h>
// Internal implementation of the context and state holder. This is not included here.
// Several low-level functions are exposed for library creators. This file is usually
// included when boost/asio.hpp is also included and calls are actually made into boost.
//

namespace ircd {
namespace ctx  {

using std::chrono::steady_clock;
using time_point = steady_clock::time_point;

IRCD_EXCEPTION(ircd::error, error)
IRCD_EXCEPTION(error, interrupted)
IRCD_EXCEPTION(error, timeout)

enum flags
{
	DEFER_POST      = 0x0001,   // Defers spawn with an ios.post()
	DEFER_DISPATCH  = 0x0002,   // (Defers) spawn with an ios.dispatch()
	DEFER_STRAND    = 0x0004,   // Defers spawn by posting to strand
	SPAWN_STRAND    = 0x0008,   // Spawn onto a strand, otherwise ios itself
	SELF_DESTRUCT   = 0x0010,   // Context deletes itself; see struct context constructor notes
	INTERRUPTED     = 0x0020,   // Marked
};

const auto DEFAULT_STACK_SIZE = 64_KiB;

// Context implementation
struct ctx;                                      // Internal implementation to hide boost headers

const int64_t &notes(const ctx &);               // Peeks at internal semaphore count (you don't need this)
const flags &flags(const ctx &);                 // Get the internal flags value.
bool finished(const ctx &);                      // Context function returned (or exception).
bool started(const ctx &);                       // Context was ever entered.
void interrupt(ctx &);                           // Interrupt the context for termination.
bool notify(ctx &);                              // Increment the semaphore (only library ppl need this)

// this_context
extern __thread struct ctx *current;             // Always set to the currently running context or null

ctx &cur();                                      // Convenience for *current (try to use this instead)
void yield();                                    // Allow other contexts to run before returning.
void wait();                                     // Returns when context notified.

// Return remaining time if notified; or <= 0 if not, and timeout thrown on throw overloads
microseconds wait(const microseconds &, const std::nothrow_t &);
template<class E, class duration> nothrow_overload<E, duration> wait(const duration &);
template<class E = timeout, class duration> throw_overload<E, duration> wait(const duration &);

// Returns false if notified; true if time point reached, timeout thrown on throw_overloads
bool wait_until(const time_point &tp, const std::nothrow_t &);
template<class E> nothrow_overload<E, bool> wait_until(const time_point &tp);
template<class E = timeout> throw_overload<E> wait_until(const time_point &tp);

// Ignores notes. Throws if interrupted.
void sleep_until(const time_point &tp);
template<class duration> void sleep(const duration &);
void sleep(const int &secs);

} // namespace ctx

using ctx::timeout;

} // namespace ircd

#include "ctx/context.h"
#include "ctx/prof.h"
#include "ctx/dock.h"
#include "ctx/mutex.h"
#include "ctx/shared_state.h"
#include "ctx/promise.h"
#include "ctx/future.h"
#include "ctx/async.h"
#include "ctx/pool.h"
#include "ctx/ole.h"

inline void
ircd::ctx::sleep(const int &secs)
{
	sleep(seconds(secs));
}

template<class duration>
void
ircd::ctx::sleep(const duration &d)
{
	sleep_until(steady_clock::now() + d);
}

template<class E>
ircd::throw_overload<E>
ircd::ctx::wait_until(const time_point &tp)
{
	if(wait_until<std::nothrow_t>(tp))
		throw E();
}

template<class E>
ircd::nothrow_overload<E, bool>
ircd::ctx::wait_until(const time_point &tp)
{
	return wait_until(tp, std::nothrow);
}

template<class E,
         class duration>
ircd::throw_overload<E, duration>
ircd::ctx::wait(const duration &d)
{
	const auto ret(wait<std::nothrow_t>(d));
	return ret <= duration(0)? throw E() : ret;
}

template<class E,
         class duration>
ircd::nothrow_overload<E, duration>
ircd::ctx::wait(const duration &d)
{
	using std::chrono::duration_cast;

	const auto ret(wait(duration_cast<microseconds>(d), std::nothrow));
	return duration_cast<duration>(ret);
}

inline ircd::ctx::ctx &
ircd::ctx::cur()
{
	assert(current);
	return *current;
}
