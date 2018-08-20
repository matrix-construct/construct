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
#define HAVE_IRCD_CTX_THIS_CTX_H

namespace ircd::ctx {
/// Interface to the currently running context
inline namespace this_ctx
{
	struct critical_indicator;                   // Indicates if yielding happened for a section
	struct critical_assertion;                   // Assert no yielding for a section
	struct exception_handler;                    // Must be present to yield in a handler
	struct uninterruptible;                      // Scope convenience for interruptible()

	struct ctx &cur();                           ///< Assumptional reference to *current

	const uint64_t &id();                        // Unique ID for cur ctx
	string_view name();                          // Optional label for cur ctx

	void wait();                                 // Returns when context is woken up.
	void yield();                                // Allow other contexts to run before returning.

	void interruption_point();                   // throws if interruption_requested()
	bool interruption_requested();               // interruption(cur())
	void interruptible(const bool &);            // interruptible(cur(), bool) +INTERRUPTION POINT
	void interruptible(const bool &, std::nothrow_t) noexcept;

	struct stack_usage_assertion;                // Assert safety factor (see ctx/prof.h)
	size_t stack_at_here() __attribute__((noinline));
	ulong cycles_here();

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
}}

namespace ircd::ctx
{
	/// Points to the currently running context or null for main stack (do not modify)
	extern __thread ctx *current;
}

/// An instance of stack_usage_assertion is placed on a ctx stack where one
/// wants to test the stack usage at both construction and destruction points
/// to ensure it is less than the value set in ctx::prof::settings which is
/// generally some engineering safety factor of 2-3 etc. This should not be
/// entirely relied upon except during debug builds, however we may try to
/// provide an optimized build mode enabling these to account for any possible
/// differences in the stack between the environments.
///
struct ircd::ctx::this_ctx::stack_usage_assertion
{
	#ifndef NDEBUG
	stack_usage_assertion();
	~stack_usage_assertion() noexcept;
	#endif
};

/// An instance of critical_assertion detects an attempt to context switch.
///
/// For when the developer specifically does not want any yielding in a
/// section or anywhere up the stack from it. This device does not prevent
/// a switch and may carry no meaning outside of debug-mode compilation. It is
/// good practice to use this device even when it appears obvious the
/// section's callgraph has no chance of yielding: code changes, and everything
/// up the graph can change without taking notice of your section.
///
class ircd::ctx::this_ctx::critical_assertion
{
	#ifndef NDEBUG
	bool theirs;

  public:
	critical_assertion();
	~critical_assertion() noexcept;
	#endif
};

/// An instance of critical_indicator reports if context switching happened.
///
/// A critical_indicator remains true after construction until a context switch
/// has occurred. It then becomes false. This is not an assertion and is
/// available in optimized builds for real use. For example, a context may
/// want to recompute some value after a context switch and opportunistically
/// skip this effort when this indicator shows no switch occurred.
///
class ircd::ctx::this_ctx::critical_indicator
{
	uint64_t state;

  public:
	uint64_t count() const             { return yields(cur()) - state;         }
	operator bool() const              { return yields(cur()) == state;        }

	critical_indicator()
	:state{yields(cur())}
	{}
};

/// An instance of exception_handler must be present to allow a context
/// switch inside a catch block. This is due to ABI limitations that stack
/// exceptions with thread-local assumptions and don't expect catch blocks
/// on the same thread to interleave when we switch the stack.
///
/// We first increment the refcount for the caught exception so it remains
/// intuitively accessible for the rest of the catch block. Then the presence
/// of this object makes the ABI believe the catch block has ended.
///
/// The exception cannot then be rethrown. DO NOT RETHROW THE EXCEPTION.
///
struct ircd::ctx::this_ctx::exception_handler
:std::exception_ptr
{
	exception_handler() noexcept;
	exception_handler(exception_handler &&) = delete;
	exception_handler(const exception_handler &) = delete;
	exception_handler &operator=(exception_handler &&) = delete;
	exception_handler &operator=(const exception_handler &) = delete;
};

/// An instance of uninterruptible will suppress interrupts sent to the
/// context for the scope. Suppression does not discard any interrupt,
/// it merely ignores it at all interruption points until the suppression
/// ends, after which it will be thrown.
///
struct ircd::ctx::this_ctx::uninterruptible
{
	struct nothrow;

	bool theirs;

	uninterruptible();
	uninterruptible(uninterruptible &&) = delete;
	uninterruptible(const uninterruptible &) = delete;
	~uninterruptible() noexcept(false);
};

/// A variant of uinterruptible for users that must guarantee the ending of
/// the suppression scope will not be an interruption point. The default
/// behavior for uninterruptible is to throw, even from its destructor, to
/// fulfill the interruption request without any more delay.
///
struct ircd::ctx::this_ctx::uninterruptible::nothrow
{
	bool theirs;

	nothrow() noexcept;
	nothrow(nothrow &&) = delete;
	nothrow(const nothrow &) = delete;
	~nothrow() noexcept;
};

/// This overload matches ::sleep() and acts as a drop-in for ircd contexts.
/// interruption point.
inline void
ircd::ctx::this_ctx::sleep(const int &secs)
{
	sleep(seconds(secs));
}

/// Yield the context for a period of time and ignore notifications. sleep()
/// is like wait() but it only returns after the timeout and not because of a
/// note.
/// interruption point.
template<class duration>
void
ircd::ctx::this_ctx::sleep(const duration &d)
{
	sleep_until(steady_clock::now() + d);
}

/// Wait for a notification until a point in time. If there is a notification
/// then context continues normally. If there's never a notification then an
/// exception (= timeout) is thrown.
/// interruption point.
template<class E>
ircd::throw_overload<E>
ircd::ctx::this_ctx::wait_until(const time_point &tp)
{
	if(wait_until<std::nothrow_t>(tp))
		throw E();
}

/// Wait for a notification until a point in time. If there is a notification
/// then returns true. If there's never a notification then returns false.
/// interruption point. this is not noexcept.
template<class E>
ircd::nothrow_overload<E, bool>
ircd::ctx::this_ctx::wait_until(const time_point &tp)
{
	return wait_until(tp, std::nothrow);
}

/// Wait for a notification for at most some amount of time. If the duration is
/// reached without a notification then E (= timeout) is thrown. Otherwise,
/// returns the time remaining on the duration.
/// interruption point
template<class E,
         class duration>
ircd::throw_overload<E, duration>
ircd::ctx::this_ctx::wait(const duration &d)
{
	const auto ret(wait<std::nothrow_t>(d));
	return ret <= duration(0)? throw E() : ret;
}

/// Wait for a notification for some amount of time. This function returns
/// when a context is notified. It always returns the duration remaining which
/// will be <= 0 to indicate a timeout without notification.
/// interruption point. this is not noexcept.
template<class E,
         class duration>
ircd::nothrow_overload<E, duration>
ircd::ctx::this_ctx::wait(const duration &d)
{
	using std::chrono::duration_cast;

	const auto ret(wait(duration_cast<microseconds>(d), std::nothrow));
	return duration_cast<duration>(ret);
}

/// Reference to the currently running context. Call if you expect to be in a
/// context. Otherwise use the ctx::current pointer.
inline ircd::ctx::ctx &
ircd::ctx::this_ctx::cur()
{
	assert(current);
	return *current;
}
