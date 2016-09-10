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

#ifdef __cplusplus
namespace ircd {
namespace ctx  {

using std::chrono::microseconds;
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
void wait();                                     // Returns when context notified.

// Return remaining time if notified; or <= 0 if not, and timeout thrown on throw overloads
microseconds wait(const microseconds &, const std::nothrow_t &);
template<class E, class duration> nothrow_overload<E, duration> wait(const duration &);
template<class E = timeout, class duration> throw_overload<E, duration> wait(const duration &);

// Returns false if notified; true if time point reached, timeout thrown on throw_overloads
bool wait_until(const time_point &tp, const std::nothrow_t &);
template<class E> nothrow_overload<E, bool> wait_until(const time_point &tp);
template<class E = timeout> throw_overload<E> wait_until(const time_point &tp);

class context
{
	std::unique_ptr<ctx> c;

  public:
	bool operator!() const                       { return !c;                                      }
	operator bool() const                        { return bool(c);                                 }

	operator const ctx &() const                 { return *c;                                      }
	operator ctx &()                             { return *c;                                      }

	bool joined() const                          { return !c || ircd::ctx::finished(*c);           }
	void interrupt()                             { ircd::ctx::interrupt(*c);                       }
	void join();                                 // Blocks the current context until this one finishes
	ctx *detach();                               // other calls undefined after this call

	// Note: Constructing with SELF_DESTRUCT flag makes any further use of this object undefined.
	context(const size_t &stack_size, std::function<void ()>, const enum flags &flags = (enum flags)0);
	context(std::function<void ()>, const enum flags &flags = (enum flags)0);
	context(context &&) noexcept = default;
	context(const context &) = delete;
	~context() noexcept;

	friend void swap(context &a, context &b) noexcept;
};

inline void
swap(context &a, context &b)
noexcept
{
	std::swap(a.c, b.c);
}

template<class E>
throw_overload<E>
wait_until(const time_point &tp)
{
	if(wait_until<std::nothrow_t>(tp))
		throw E();
}

template<class E>
nothrow_overload<E, bool>
wait_until(const time_point &tp)
{
	return wait_until(tp, std::nothrow);
}

template<class E,
         class duration>
throw_overload<E, duration>
wait(const duration &d)
{
	const auto ret(wait<std::nothrow_t>(d));
	return ret <= duration(0)? throw E() : ret;
}

template<class E,
         class duration>
nothrow_overload<E, duration>
wait(const duration &d)
{
	using std::chrono::duration_cast;

	const auto ret(wait(duration_cast<microseconds>(d), std::nothrow));
	return duration_cast<duration>(ret);
}

inline ctx &
cur()
{
	assert(current);
	return *current;
}

}      // namespace ctx

using ctx::context;
using ctx::timeout;

}      // namespace ircd
#endif // __cplusplus
