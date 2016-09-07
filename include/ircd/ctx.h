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

IRCD_EXCEPTION(ircd::error, error)
IRCD_EXCEPTION(error, interrupted)
IRCD_EXCEPTION(error, timeout)

enum flags
{
	DEFER_POST      = 0x01,   // Defers spawn with an ios.post()
	DEFER_DISPATCH  = 0x02,   // (Defers) spawn with an ios.dispatch()
	DEFER_STRAND    = 0x04,   // Defers spawn by posting to strand
	SPAWN_STRAND    = 0x08,   // Spawn onto a strand, otherwise ios itself
};

const auto DEFAULT_STACK_SIZE = 64_KiB;

struct ctx;                                      // Internal implementation to hide boost headers

extern __thread struct ctx *current;             // Always set to the currently running context or null

ctx &cur();                                      // Convenience for *current (try to use this instead)
void free(const ctx *);                          // Manual delete (for the incomplete type)
const int64_t &notes(const ctx &);               // Peeks at internal semaphore count (you don't need this)
bool started(const ctx &);                       // Context was ever entered. (can't know if finished)
bool notify(ctx &);                              // Increment the semaphore (only library ppl need this)

class context
{
	std::unique_ptr<ctx> c;

  public:
	operator const ctx &() const                 { return *c;                                      }
	operator ctx &()                             { return *c;                                      }

	bool joined() const                          { return !c || ircd::ctx::finished(*c);           }
	void interrupt()                             { ircd::ctx::interrupt(*c);                       }
	void join();                                 // Blocks the current context until this one finishes
	ctx *detach();                               // other calls undefined after this call

	context(const size_t &stack_size, std::function<void ()>, const flags &flags = (enum flags)0);
	context(std::function<void ()>, const flags &flags = (enum flags)0);
	context(context &&) noexcept = default;
	context(const context &) = delete;
	~context() noexcept;

	friend void swap(context &a, context &b) noexcept;
};

// "namespace this_context;" interface
std::chrono::microseconds wait(const std::chrono::microseconds &);
std::chrono::microseconds wait();

template<class duration>
duration wait(const duration &d)
{
	using std::chrono::duration_cast;

	const auto us(duration_cast<std::chrono::microseconds>(d));
	return duration_cast<duration>(wait(us));
}

template<class E>
nothrow_overload<E, bool>
wait_until(const std::chrono::steady_clock::time_point &tp)
{
	static const auto zero(tp - tp);
	return wait(tp - std::chrono::steady_clock::now()) <= zero;
}

template<class E = timeout>
throw_overload<E>
wait_until(const std::chrono::steady_clock::time_point &tp)
{
	if(wait_until<std::nothrow_t>(tp))
		throw E();
}

template<class E = timeout>
throw_overload<E>
wait_until()
{
	wait_until<E>(std::chrono::steady_clock::time_point::max());
}

inline
void swap(context &a, context &b)
noexcept
{
	std::swap(a.c, b.c);
}

inline
ctx &cur()
{
	assert(current);
	return *current;
}

}      // namespace ctx

using ctx::context;
using ctx::timeout;

}      // namespace ircd
#endif // __cplusplus
