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
#define HAVE_IRCD_CTX_CTX_H

///////////////////////////////////////////////////////////////////////////////
//
// low-level ctx interface exposure
//
namespace ircd::ctx
{
	struct ctx;

	IRCD_OVERLOAD(threadsafe)

	const uint64_t &id(const ctx &);             // Unique ID for context
	string_view name(const ctx &);               // User's optional label for context
	const int64_t &notes(const ctx &);           // Peeks at internal semaphore count
	bool finished(const ctx &);                  // Context function returned (or exception).
	bool started(const ctx &);                   // Context was ever entered.

	void interrupt(ctx &);                       // Interrupt the context for termination.
	void strand(ctx &, std::function<void ()>);  // Post function to context strand
	void notify(ctx &, threadsafe_t);            // Notify context with threadsafety.
	bool notify(ctx &);                          // Queue a context switch
	void yield(ctx &);                           // Direct context switch
}

///////////////////////////////////////////////////////////////////////////////
//
// "this_context" interface relevant to the currently running context
//
namespace ircd::ctx
{
	extern __thread struct ctx *current;         // Always set to the currently running context or null

	ctx &cur();                                  // Convenience for *current (try to use this instead)
	void yield();                                // Allow other contexts to run before returning.
	void wait();                                 // Returns when context notified.

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
}

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
