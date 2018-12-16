// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <RB_INC_X86INTRIN_H
#include <cxxabi.h>
#include <ircd/asio.h>
#include "ctx.h"

/// Instance list linkage for the list of all ctx instances.
template<>
decltype(ircd::util::instance_list<ircd::ctx::ctx>::list)
ircd::util::instance_list<ircd::ctx::ctx>::list
{};

/// Public interface linkage for the list of all ctx instances
decltype(ircd::ctx::ctxs)
ircd::ctx::ctxs
{
	ctx::ctx::list
};

/// Monotonic ctx id counter state. This counter is incremented for each
/// newly created context.
decltype(ircd::ctx::ctx::id_ctr)
ircd::ctx::ctx::id_ctr
{
	0
};

/// Spawn (internal)
void
ircd::ctx::spawn(ctx *const c,
                 context::function func)
{
	const boost::coroutines::attributes attrs
	{
		c->stack.max,
		boost::coroutines::stack_unwind
	};

	auto bound
	{
		std::bind(&ctx::operator(), c, ph::_1, std::move(func))
	};

	boost::asio::spawn(c->strand, std::move(bound), attrs);
}

// linkage for dtor
ircd::ctx::ctx::~ctx()
noexcept
{
}

/// Base frame for a context.
///
/// This function is the first thing executed on the new context's stack
/// and calls the user's function.
void
ircd::ctx::ctx::operator()(boost::asio::yield_context yc,
                           const std::function<void ()> func)
noexcept try
{
	this->yc = &yc;
	notes = 1;
	stack.base = uintptr_t(__builtin_frame_address(0));
	ircd::ctx::current = this;
	mark(prof::event::CUR_ENTER);

	const unwind atexit([this]
	{
		mark(prof::event::CUR_LEAVE);
		adjoindre.notify_all();
		ircd::ctx::current = nullptr;
		this->yc = nullptr;

		if(flags & context::DETACH)
			delete this;
	});

	// Check for a precocious interrupt
	if(unlikely(flags & (context::INTERRUPTED | context::TERMINATED)))
		return;

	if(likely(bool(func)))
		func();
}
catch(const ircd::ctx::interrupted &)
{
	return;
}
catch(const ircd::ctx::terminated &)
{
	return;
}
catch(const std::exception &e)
{
	log::critical
	{
		"ctx('%s' id:%u): unhandled: %s",
		name,
		id,
		e.what()
	};

	// Preserving the stacktrace from the throw point here is hopeless.
	// We can terminate for developer nuisance but we will never know
	// where this exception came from and where it is going. Bottom line
	// is that #ifdef'ing away this handler or rethrowing isn't as useful as
	// handling the exception here with a log message and calling it a day.
	return;
}

/// Direct context switch to this context.
///
/// This currently doesn't work yet because the suspension state of this
/// context has to be ready to be jumped to and that isn't implemented yet.
void
ircd::ctx::ctx::jump()
{
	assert(this->yc);
	assert(current != this);                  // can't jump to self

	auto &yc(*this->yc);
	auto &target(*yc.coro_.lock());

	// Jump from the currently running context (source) to *this (target)
	// with continuation of source after target
	{
		current->notes = 0; // Unconditionally cleared here
		const continuation continuation;
		target();
	}

	assert(current != this);
	assert(current->notes == 1); // notes = 1; set by continuation dtor on wakeup

	interruption_point();
}

/// Yield (suspend) this context until notified.
///
/// This context must be currently running otherwise bad things. Returns false
/// if the context was notified before actually suspending; the note is then
/// considered handled an another attempt to `wait()` can be made. Returns true
/// if the context suspended and was notified. When a context wakes up the
/// note counter is reset.
bool
ircd::ctx::ctx::wait()
{
	namespace errc = boost::system::errc;

	assert(this->yc);
	assert(current == this);

	if(--notes > 0)
		return false;

	// An interrupt invokes this closure to force the alarm to return.
	const interruptor interruptor{[this]
	(ctx *const &interruptor) noexcept
	{
		wake();
	}};

	// This is currently a dummy predicate; this is where we can take the
	// user's real wakeup condition (i.e from a ctx::dock) and use it with
	// an internal scheduler.
	const predicate &predicate{[this]
	{
		return notes > 0;
	}};

	// The register switch itself occurs inside the alarm.async_wait() call.
	// The construction of the arguments to the call on this stack comprise
	// our final control before the context switch. The destruction of the
	// arguments comprise the initial control after the context switch.
	boost::system::error_code ec;
	alarm.async_wait(yield_context{continuation{predicate, interruptor}}[ec]);

	assert(ec == errc::operation_canceled || ec == errc::success);
	assert(current == this);
	assert(notes == 1);  // notes = 1; set by continuation dtor on wakeup

	interruption_point();
	return true;
}

/// Notifies this context to resume (wake up from waiting).
///
/// Returns true if this note was the first note received by this context
/// while it's been suspended or false if it's already been notified.
bool
ircd::ctx::ctx::note()
{
	if(notes++ > 0)
		return false;

	if(this == current)
		return true;

	return wake();
}

/// Wakes a context without a note (internal)
bool
ircd::ctx::ctx::wake()
try
{
	alarm.cancel();
	return true;
}
catch(const boost::system::system_error &e)
{
	log::error
	{
		"ctx::wake(%p): %s", this, e.what()
	};

	return false;
}

/// Throws if this context has been flagged for interruption and clears
/// the flag.
void
ircd::ctx::ctx::interruption_point()
{
	static const auto &flags
	{
		context::TERMINATED | context::INTERRUPTED
	};

	if(likely((this->flags & flags) == 0))
		return;

	if(unlikely(termination_point(std::nothrow)))
		throw terminated{};

	if(unlikely(interruption_point(std::nothrow)))
		throw interrupted
		{
			"ctx(%p) '%s'", (const void *)this, name
		};
}

/// Returns true if this context has been flagged for termination.
/// Does not clear the flag.
bool
ircd::ctx::ctx::termination_point(std::nothrow_t)
{
	if(unlikely(flags & context::TERMINATED))
	{
		// see: interruption_point().
		if(flags & context::NOINTERRUPT)
			return false;

		mark(prof::event::CUR_TERMINATE);
		assert(flags & ~context::NOINTERRUPT);
		return true;
	}
	else return false;
}

/// Returns true if this context has been flagged for interruption and
/// clears the flag.
bool
ircd::ctx::ctx::interruption_point(std::nothrow_t)
{
	// Interruption shouldn't be used for normal operation,
	// so please eat this branch misprediction.
	if(unlikely(flags & context::INTERRUPTED))
	{
		// The NOINTERRUPT flag works by pretending there is no INTERRUPTED
		// flag set and also does not clear the flag. This allows the interrupt
		// to remaing pending until the uninterruptible section is complete.
		if(flags & context::NOINTERRUPT)
			return false;

		flags &= ~context::INTERRUPTED;
		mark(prof::event::CUR_INTERRUPT);
		assert(~flags & context::NOINTERRUPT);
		return true;
	}
	else return false;
}

bool
ircd::ctx::ctx::started()
const
{
	return stack.base != 0;
}

bool
ircd::ctx::ctx::finished()
const
{
	return started() && yc == nullptr;
}

///////////////////////////////////////////////////////////////////////////////
//
// ctx/ctx.h
//

/// Yield to context `ctx`.
///
///
void
ircd::ctx::yield(ctx &ctx)
{
	assert(current);

	//ctx.jump();
	// !!! TODO !!!
	// XXX: We can't jump directly to a context if it's waiting on its alarm, and
	// we don't know whether it's waiting on its alarm. We can add another flag to
	// inform us of that, but most contexts are usually waiting on their alarm anyway.
	//
	// Perhaps a better way to do this would be to centralize the alarms into a single
	// context with the sole job of waiting on a single alarm. Then it can schedule
	// things allowing for more direct jumps until all work is complete.
	// !!! TODO !!!

	notify(ctx);
}

/// Notifies `ctx` to wake up from another std::thread
void
ircd::ctx::notify(ctx &ctx,
                  threadsafe_t)
{
	signal(ctx, [&ctx]
	{
		notify(ctx);
	});
}

/// Notifies `ctx` to wake up. This will enqueue the resumption, not jump
/// directly to `ctx`.
bool
ircd::ctx::notify(ctx &ctx)
{
	return ctx.note();
}

/// Executes `func` sometime between executions of `ctx` with thread-safety
/// so `func` and `ctx` are never executed concurrently no matter how many
/// threads the io_service has available to execute events on.
void
ircd::ctx::signal(ctx &ctx,
                  std::function<void ()> func)
{
	ctx.strand.post(std::move(func));
}

/// Marks `ctx` for termination. Terminate is similar to interrupt() but the
/// exception thrown is ctx::terminate which does not participate in the
/// std::exception hierarchy. Project code is unlikely to catch this.
void
ircd::ctx::terminate(ctx &ctx)
{
	if(finished(ctx))
		return;

	if(termination(ctx))
		return;

	ctx.flags |= context::TERMINATED;
	if(!interruptible(ctx))
		return;

	if(likely(&ctx != current && ctx.cont != nullptr))
		ctx.cont->intr(current);
}

/// Marks `ctx` for interruption and enqueues it for resumption to receive the
/// interrupt which will be an exception coming out of the point where the
/// `ctx` was yielding.
void
ircd::ctx::interrupt(ctx &ctx)
{
	if(unlikely(ircd::runlevel == runlevel::QUIT))
		return terminate(ctx);

	if(finished(ctx))
		return;

	if(interruption(ctx))
		return;

	ctx.flags |= context::INTERRUPTED;
	if(!interruptible(ctx))
		return;

	if(likely(&ctx != current && ctx.cont != nullptr))
		ctx.cont->intr(current);
}

/// Marks `ctx` for whether to allow or suppress interruption. Suppression
/// does not ignore an interrupt itself, it only ignores the interruption
/// points. Thus when a suppression ends if the interrupt flag was ever set
/// the next interruption point will throw as expected.
void
ircd::ctx::interruptible(ctx &ctx,
                         const bool &b)
{
	if(b)
		ctx.flags &= ~context::NOINTERRUPT;
	else
		ctx.flags |= context::NOINTERRUPT;
}

/// started() && !finished() && !running
bool
ircd::ctx::waiting(const ctx &ctx)
{
	return started(ctx) && !finished(ctx) && !running(ctx);
}

/// Indicates if `ctx` is the current ctx
bool
ircd::ctx::running(const ctx &ctx)
{
	return &ctx == current;
}

/// Indicates if `ctx` was ever jumped to
bool
ircd::ctx::started(const ctx &ctx)
{
	return ctx.started();
}

/// Indicates if the base frame for `ctx` returned
bool
ircd::ctx::finished(const ctx &ctx)
{
	return ctx.finished();
}

/// Indicates if `ctx` was terminated; does not clear the flag
bool
ircd::ctx::termination(const ctx &c)
noexcept
{
	return c.flags & context::TERMINATED;
}

/// Indicates if `ctx` was interrupted; does not clear the flag
bool
ircd::ctx::interruption(const ctx &c)
noexcept
{
	return c.flags & context::INTERRUPTED;
}

/// Indicates if `ctx` will suppress any interrupts.
bool
ircd::ctx::interruptible(const ctx &c)
noexcept
{
	return ~c.flags & context::NOINTERRUPT;
}

/// Returns the cycle count for `ctx`
const ulong &
ircd::ctx::cycles(const ctx &ctx)
{
	return ctx.profile.cycles;
}

/// Returns the yield count for `ctx`
const uint64_t &
ircd::ctx::yields(const ctx &ctx)
{
	return ctx.profile.yields;
}

/// Returns the notification count for `ctx`
const int32_t &
ircd::ctx::notes(const ctx &ctx)
{
	return ctx.notes;
}

/// Returns the notification count for `ctx`
const size_t &
ircd::ctx::stack_at(const ctx &ctx)
{
	return ctx.stack.at;
}

/// Returns the notification count for `ctx`
const size_t &
ircd::ctx::stack_max(const ctx &ctx)
{
	return ctx.stack.max;
}

/// Returns the developer's optional name literal for `ctx`
ircd::string_view
ircd::ctx::name(const ctx &ctx)
{
	return ctx.name;
}

/// Returns a reference to unique ID for `ctx` (which will go away with `ctx`)
const uint64_t &
ircd::ctx::id(const ctx &ctx)
{
	return ctx.id;
}

///////////////////////////////////////////////////////////////////////////////
//
// ctx/this_ctx.h
//

// set by the continuation object and the base frame.
__thread ircd::ctx::ctx *
ircd::ctx::current;

/// Yield the currently running context until `time_point` ignoring notes
void
ircd::ctx::this_ctx::sleep_until(const steady_clock::time_point &tp)
{
	while(!wait_until(tp, std::nothrow));
}

/// Yield the currently running context until notified or `time_point`.
///
/// Returns true if this function returned because `time_point` was hit or
/// false because this context was notified.
bool
ircd::ctx::this_ctx::wait_until(const steady_clock::time_point &tp,
                                const std::nothrow_t &)
{
	auto &c(cur());
	c.alarm.expires_at(tp);
	c.wait(); // now you're yielding with portals

	return steady_clock::now() >= tp;
}

/// Yield the currently running context for `duration` or until notified.
///
/// Returns the duration remaining if notified, or <= 0 if suspended for
/// the full duration, or unchanged if no suspend ever took place.
ircd::microseconds
ircd::ctx::this_ctx::wait(const microseconds &duration,
                          const std::nothrow_t &)
{
	auto &c(cur());
	c.alarm.expires_from_now(duration);
	c.wait(); // now you're yielding with portals
	const auto ret(c.alarm.expires_from_now());

	// return remaining duration.
	// this is > 0 if notified
	// this is unchanged if a note prevented any wait at all
	return duration_cast<microseconds>(ret);
}

/// Yield the currently running context until notified.
void
ircd::ctx::this_ctx::wait()
{
	auto &c(cur());
	c.alarm.expires_at(steady_clock::time_point::max());
	c.wait(); // now you're yielding with portals
}

/// Post the currently running context to the event queue and then suspend to
/// allow other contexts in the queue to run.
///
/// Until we have our own queue the ios queue makes no guarantees if the queue
/// is FIFO or LIFO etc :-/ It is generally bad practice to use this function,
/// as one should make the effort to devise a specific cooperative strategy for
/// how context switching occurs rather than this coarse/brute technique.
void
ircd::ctx::this_ctx::yield()
{
	bool done(false);
	const auto restore([&done, &me(cur())]
	{
		done = true;
		notify(me);
	});

	// All spurious notifications are ignored until `done`
	ircd::post(restore); do
	{
		wait();
	}
	while(!done);
}

ulong
ircd::ctx::this_ctx::cycles_here()
{
	assert(current);
	return cycles(cur()) + prof::cur_slice_cycles();
}

size_t
ircd::ctx::this_ctx::stack_at_here()
{
	assert(current);
	return cur().stack.base - uintptr_t(__builtin_frame_address(0));
}

/// Throws interrupted if the currently running context was interrupted
/// and clears the interrupt flag.
void
ircd::ctx::this_ctx::interruptible(const bool &b)
{
	const bool theirs
	{
		interruptible(cur())
	};

	if(theirs && !b)
		interruption_point();

	interruptible(cur(), b);

	if(!theirs && b)
		interruption_point();
}

void
ircd::ctx::this_ctx::interruptible(const bool &b,
                                   std::nothrow_t)
noexcept
{
	interruptible(cur(), b);
}

bool
ircd::ctx::this_ctx::interruptible()
noexcept
{
	return interruptible(cur());
}

/// Throws interrupted if the currently running context was interrupted
/// and clears the interrupt flag.
void
ircd::ctx::this_ctx::interruption_point()
{
	// Asserting to know if this call is useless as it's being made in
	// an uninterruptible scope anyway. It's okay to relax this assertion.
	assert(interruptible());
	return cur().interruption_point();
}

/// Returns true if the currently running context was interrupted and clears
/// the interrupt flag.
bool
ircd::ctx::this_ctx::interruption_requested()
{
	return interruption(cur()) || termination(cur());
}

/// Returns unique ID of currently running context
const uint64_t &
ircd::ctx::this_ctx::id()
{
	static const uint64_t zero{0};
	return current? id(cur()) : zero;
}

/// Returns optional developer-given name for currently running context
ircd::string_view
ircd::ctx::this_ctx::name()
{
	static const string_view nada{"*"};
	return current? name(cur()) : nada;
}

//
// uinterruptible
//

ircd::ctx::this_ctx::uninterruptible::uninterruptible()
:theirs
{
	interruptible(cur())
}
{
	interruptible(false);
}

ircd::ctx::this_ctx::uninterruptible::~uninterruptible()
noexcept(false)
{
	interruptible(theirs);
}

//
// uninterruptible::nothrow
//

ircd::ctx::this_ctx::uninterruptible::nothrow::nothrow()
noexcept
:theirs
{
	interruptible(cur())
}
{
	interruptible(false, std::nothrow);
}

ircd::ctx::this_ctx::uninterruptible::nothrow::~nothrow()
noexcept
{
	interruptible(theirs, std::nothrow);
}

//
// exception_handler
//

ircd::ctx::this_ctx::exception_handler::exception_handler()
noexcept
:std::exception_ptr{std::current_exception()}
{
	assert(bool(*this));
	//assert(!std::uncaught_exceptions());
	__cxa_end_catch();

	// We don't yet support more levels of exceptions; after ending this
	// catch we can't still be in another one. This doesn't apply if we're
	// not on any ctx currently.
	assert(!current || !std::current_exception());
}

//
// critical_assertion
//

namespace ircd::ctx
{
	bool critical_asserted;

	void assert_critical();
}

#ifndef NDEBUG
ircd::ctx::this_ctx::critical_assertion::critical_assertion()
:theirs{critical_asserted}
{
	critical_asserted = true;
}
#endif

#ifndef NDEBUG
ircd::ctx::this_ctx::critical_assertion::~critical_assertion()
noexcept
{
	assert(critical_asserted);
	critical_asserted = theirs;
}
#endif

#ifndef NDEBUG
void
ircd::ctx::assert_critical()
{
	if(unlikely(critical_asserted))
		throw ircd::assertive
		{
			"%lu '%s' :Illegal context switch", id(), name()
		};
}
#else
void
ircd::ctx::assert_critical()
{

}
#endif

//
// stack_usage_assertion
//

#ifndef NDEBUG
ircd::ctx::this_ctx::stack_usage_assertion::stack_usage_assertion()
{
	const auto stack_usage(stack_at_here());
	assert(stack_usage < cur().stack.max * double(prof::settings::stack_usage_assertion));
}
#endif

#ifndef NDEBUG
ircd::ctx::this_ctx::stack_usage_assertion::~stack_usage_assertion()
noexcept
{
	const auto stack_usage(stack_at_here());
	assert(stack_usage < cur().stack.max * double(prof::settings::stack_usage_assertion));
}
#endif

///////////////////////////////////////////////////////////////////////////////
//
// ctx/continuation.h
//

decltype(ircd::ctx::continuation::true_predicate)
ircd::ctx::continuation::true_predicate{[]
() -> bool
{
	return true;
}};

decltype(ircd::ctx::continuation::false_predicate)
ircd::ctx::continuation::false_predicate{[]
() -> bool
{
	return false;
}};

decltype(ircd::ctx::continuation::noop_interruptor)
ircd::ctx::continuation::noop_interruptor{[]
(ctx *const &interruptor) -> void
{
	return;
}};

//
// continuation
//

ircd::ctx::continuation::continuation(const predicate &pred,
                                      const interruptor &intr)
:self
{
	ircd::ctx::current
}
,pred
{
	pred
}
,intr
{
	intr
}
{
	assert_critical();
	assert(!critical_asserted);
	assert(self != nullptr);
	assert(self->notes <= 1);

	// Note: Construct an instance of ctx::exception_handler to enable yielding
	// in your catch block.
	//
	// GNU cxxabi uses a singly-linked forward list (aka the 'exception
	// stack') for pending exception activities. Due to this limitation we
	// cannot interleave _cxa_begin_catch() and __cxa_end_catch() by yielding
	// the ircd::ctx in an exception handler.
	assert(!std::current_exception());
	//assert(!std::uncaught_exceptions());

	mark(prof::event::CUR_YIELD);
	self->profile.yields++;
	self->cont = this;
	ircd::ctx::current = nullptr;
}

ircd::ctx::continuation::~continuation()
noexcept
{
	ircd::ctx::current = self;
	self->notes = 1;
	mark(prof::event::CUR_CONTINUE);

	// self->continuation is not null'ed here; it remains an invalid
	// pointer while the context is awake.
}

ircd::ctx::continuation::operator boost::asio::yield_context &()
{
	return *self->yc;
}

ircd::ctx::continuation::operator const boost::asio::yield_context &()
const
{
	return *self->yc;
}

//
// to_asio
//

ircd::ctx::to_asio::to_asio(const interruptor &intr)
:continuation
{
	false_predicate, intr
}
{
}

///////////////////////////////////////////////////////////////////////////////
//
// ctx/context.h
//

// Linkage here for default construction because ctx is internal.
ircd::ctx::context::context()
{
}

ircd::ctx::context::context(const char *const &name,
                            const size_t &stack_sz,
                            const flags &flags,
                            function func)
:c
{
	std::make_unique<ctx>(name, stack_sz, flags, ios::get())
}
{
	auto spawn
	{
		std::bind(&ircd::ctx::spawn, c.get(), std::move(func))
	};

	// The profiler is told about the spawn request here, not inside the closure
	// which is probably the same event-slice as event::CUR_ENTER and not as useful.
	mark(prof::event::SPAWN);

	// When the user passes the DETACH flag we want to release the unique_ptr
	// of the ctx if and only if that ctx is committed to freeing itself. Our
	// commitment ends at the 180 of this function. If no exception was thrown
	// we expect the context to be committed to entry. If the POST flag is
	// supplied and it gets lost in the asio queue it will not be entered, and
	// will not be able to free itself; that will leak.
	const unwind::nominal release
	{
		[this, &flags]
		{
			if(flags & context::DETACH)
				this->detach();
		}
	};

	if(flags & POST)
	{
		ios::post(std::move(spawn));
		return;
	}

	// The current context must be reasserted if spawn returns here
	auto *const theirs(ircd::ctx::current);
	const unwind recurrent([&theirs]
	{
		ircd::ctx::current = theirs;
	});

	if(flags & DISPATCH)
		ios::dispatch(std::move(spawn));
	else
		spawn();
}

ircd::ctx::context::context(const char *const &name,
                            const size_t &stack_size,
                            function func,
                            const flags &flags)
:context
{
	name, stack_size, flags, std::move(func)
}
{
}

ircd::ctx::context::context(const char *const &name,
                            const flags &flags,
                            function func)
:context
{
	name, DEFAULT_STACK_SIZE, flags, std::move(func)
}
{
}

ircd::ctx::context::context(const char *const &name,
                            function func,
                            const flags &flags)
:context
{
	name, DEFAULT_STACK_SIZE, flags, std::move(func)
}
{
}

ircd::ctx::context::context(function func,
                            const flags &flags)
:context
{
	"<noname>", DEFAULT_STACK_SIZE, flags, std::move(func)
}
{
}

ircd::ctx::context::context(context &&other)
noexcept
:c{std::move(other.c)}
{
}

ircd::ctx::context &
ircd::ctx::context::operator=(context &&other)
noexcept
{
	std::swap(this->c, other.c);
	return *this;
}

ircd::ctx::context::~context()
noexcept
{
	if(!c)
		return;

	// Can't join to bare metal, only from within another context.
	if(current)
	{
		const uninterruptible::nothrow ui;
		terminate();
		join();
		return;
	}

	// because *this uses unique_ptr's, if we dtor the ircd::ctx from
	// right here and ircd::ctx hasn't been entered yet because the user
	// passed the POST flag, the ctx::spawn() is still sitting in the ios
	// queue.
	if(!started(*c))
	{
		detach();
		return;
	}

	// When this is bare metal the above join branch will not have been
	// taken. In that case we should detach the context so it frees itself,
	// but only if the context has not already finished.
	if(!current && !finished(*c))
	{
		detach();
		return;
	}
}

void
ircd::ctx::context::join()
{
	if(joined())
		return;

	assert(bool(c));
	mark(prof::event::JOIN);
	c->adjoindre.wait([this]
	{
		return joined();
	});

	mark(prof::event::JOINED);
}

ircd::ctx::ctx *
ircd::ctx::context::detach()
{
	assert(bool(c));
	c->flags |= DETACH;
	return c.release();
}

///////////////////////////////////////////////////////////////////////////////
//
// ctx_pool.h
//

ircd::ctx::pool::pool(const char *const &name,
                      const size_t &stack_size,
                      const size_t &size)
:name{name}
,stack_size{stack_size}
,running{0}
,working{0}
{
	add(size);
}

ircd::ctx::pool::~pool()
noexcept
{
	terminate();
	join();

	assert(ctxs.empty());
	assert(q.empty());
}

void
ircd::ctx::pool::operator()(closure closure)
{
	q.push(std::move(closure));
}

void
ircd::ctx::pool::set(const size_t &num)
{
	if(size() > num)
		del(size() - num);
	else
		add(num - size());
}

void
ircd::ctx::pool::del(const size_t &num)
{
	const auto requested
	{
		ssize_t(size()) - ssize_t(num)
	};

	const auto target
	{
		size_t(std::max(requested, 0L))
	};

	while(ctxs.size() > target)
		ctxs.pop_back();
}

void
ircd::ctx::pool::add(const size_t &num)
{
	for(size_t i(0); i < num; ++i)
		ctxs.emplace_back(name, stack_size, context::POST, std::bind(&pool::main, this));
}

void
ircd::ctx::pool::join()
{
	set(0);
}

void
ircd::ctx::pool::interrupt()
{
	for(auto &context : ctxs)
		context.interrupt();
}

void
ircd::ctx::pool::terminate()
{
	for(auto &context : ctxs)
		context.terminate();
}

void
ircd::ctx::pool::main()
noexcept try
{
	++running;
	const unwind avail([this]
	{
		--running;
	});

	while(1)
		next();
}
catch(const interrupted &e)
{
//	log::debug
//	{
//		"pool(%p) ctx(%p): %s", this, &cur(), e.what()
//	};
}
catch(const terminated &e)
{
//	log::debug
//	{
//		"pool(%p) ctx(%p): %s", this, &cur(), e.what()
//	};
}

void
ircd::ctx::pool::next()
try
{
	const auto func
	{
		std::move(q.pop())
	};

	++working;
	const unwind avail([this]
	{
		--working;
	});

	func();
}
catch(const interrupted &e)
{
	// Interrupt is stopped here so this ctx can be reused for a new job.
	return;
}
catch(const std::exception &e)
{
	log::critical
	{
		"pool(%p) ctx(%p '%s' id:%u): unhandled: %s",
		this,
		current,
		ircd::ctx::name(cur()),
		ircd::ctx::id(cur()),
		e.what()
	};
}

void
ircd::ctx::debug_stats(const pool &pool)
{
	log::debug
	{
		"pool '%s' (stack size: %zu) total: %zu avail: %zu queued: %zu active: %zu pending: %zu",
		pool.name,
		pool.stack_size,
		pool.size(),
		pool.avail(),
		pool.queued(),
		pool.active(),
		pool.pending()
	};
}

///////////////////////////////////////////////////////////////////////////////
//
// ctx_prof.h
//

namespace ircd::ctx::prof
{
	ulong _slice_start;      // Time slice state
	ulong _slice_total;      // Monotonic accumulator

	void check_stack();
	void check_slice();
	void slice_start();

	void handle_cur_continue();
	void handle_cur_yield();
	void handle_cur_leave();
	void handle_cur_enter();
}

// stack_usage_warning at 1/3 engineering tolerance
decltype(ircd::ctx::prof::settings::stack_usage_warning)
ircd::ctx::prof::settings::stack_usage_warning
{
	{ "name",     "ircd.ctx.prof.stack_usage_warning" },
	{ "default",  0.33                                },
};

// stack_usage_assertion at 1/2 engineering tolerance
decltype(ircd::ctx::prof::settings::stack_usage_assertion)
ircd::ctx::prof::settings::stack_usage_assertion
{
	{ "name",     "ircd.ctx.prof.stack_usage_assertion" },
	{ "default",  0.50                                  },
};

// slice_warning after this number of tsc ticks...
decltype(ircd::ctx::prof::settings::slice_warning)
ircd::ctx::prof::settings::slice_warning
{
	{ "name",     "ircd.ctx.prof.slice_warning" },
	{ "default",  280 * 1000000L                },
};

// slice_interrupt after this number of tsc ticks...
decltype(ircd::ctx::prof::settings::slice_interrupt)
ircd::ctx::prof::settings::slice_interrupt
{
	{ "name",     "ircd.ctx.prof.slice_interrupt" },
	{ "default",  0L                              },
	{ "persist",  false                           },
};

// slice_assertion after this number of tsc ticks...
decltype(ircd::ctx::prof::settings::slice_assertion)
ircd::ctx::prof::settings::slice_assertion
{
	{ "name",     "ircd.ctx.prof.slice_assertion" },
	{ "default",  0L                              },
	{ "persist",  false                           },
};

#ifdef RB_DEBUG
void
ircd::ctx::prof::mark(const event &e)
{
	switch(e)
	{
		case event::CUR_ENTER:       handle_cur_enter();     break;
		case event::CUR_LEAVE:       handle_cur_leave();     break;
		case event::CUR_YIELD:       handle_cur_yield();     break;
		case event::CUR_CONTINUE:    handle_cur_continue();  break;
		default:                                             break;
	}
}
#else
void
ircd::ctx::prof::mark(const event &e)
{
}
#endif

ulong
ircd::ctx::prof::cur_slice_cycles()
{
	return rdtsc() - cur_slice_start();
}

const ulong &
ircd::ctx::prof::cur_slice_start()
{
	return _slice_start;
}

const ulong &
ircd::ctx::prof::total_slice_cycles()
{
	return _slice_total;
}

void
ircd::ctx::prof::handle_cur_enter()
{
	slice_start();
}

void
ircd::ctx::prof::handle_cur_leave()
{
	check_slice();
}

void
ircd::ctx::prof::handle_cur_yield()
{
	check_slice();
	check_stack();
}

void
ircd::ctx::prof::handle_cur_continue()
{
	slice_start();
}

void
ircd::ctx::prof::slice_start()
{
	_slice_start = rdtsc();
}

void
ircd::ctx::prof::check_slice()
{
	auto &c(cur());
	const bool slice_exempt
	{
		c.flags & context::SLICE_EXEMPT
	};

	const auto &last_cycles
	{
		cur_slice_cycles()
	};

	c.profile.cycles += last_cycles;
	_slice_total += last_cycles;

	const ulong &slice_warning(settings::slice_warning);
	if(unlikely(slice_warning > 0 && last_cycles >= slice_warning && !slice_exempt))
		log::dwarning
		{
			"context '%s' id:%lu watchdog: timeslice excessive; lim:%lu last:%lu pct:%.2lf",
			name(c),
			id(c),
			slice_warning,
			last_cycles,
			((double(last_cycles) / double(slice_warning)) * 100.0)
		};

	const ulong &slice_assertion(settings::slice_assertion);
	assert(slice_assertion == 0 || last_cycles < slice_assertion || slice_exempt);

	const ulong &slice_interrupt(settings::slice_interrupt);
	if(unlikely(slice_interrupt > 0 && last_cycles >= slice_interrupt && !slice_exempt))
		throw interrupted
		{
			"context '%s' id:%lu watchdog interrupt; lim:%lu last:%lu total:%lu",
			name(c),
			id(c),
			slice_interrupt,
			last_cycles,
			cycles(c),
		};
}

void
ircd::ctx::prof::check_stack()
{
	auto &c(cur());
	const bool stack_exempt
	{
		c.flags & context::STACK_EXEMPT
	};

	const double &stack_max(c.stack.max);
	const auto &stack_at(stack_at_here());
	c.stack.at = stack_at;

	const double &stack_usage_assertion(settings::stack_usage_assertion);
	if(unlikely(!stack_exempt && stack_at > stack_max * stack_usage_assertion))
	{
		log::dwarning
		{
			"context '%s' id:%lu watchdog: stack used %zu of %zu bytes",
			name(c),
			id(c),
			stack_at,
			c.stack.max
		};

		const double &stack_usage_assertion(settings::stack_usage_assertion);
		assert(stack_at < c.stack.max * double(settings::stack_usage_assertion));
	}
}

#ifdef HAVE_X86INTRIN_H
ulong
ircd::ctx::prof::rdtsc()
{
	return __rdtsc();
}
#else
ulong
ircd::ctx::prof::rdtsc()
{
	static_assert
	(
		0, "TODO: Implement fallback here"
	);

	return 0;
}
#endif

///////////////////////////////////////////////////////////////////////////////
//
// ctx_ole.h
//

namespace ircd::ctx::ole
{
	using closure = std::function<void ()>;

	extern conf::item<size_t> thread_max;

	std::mutex mutex;
	std::condition_variable cond;
	bool termination;
	std::deque<closure> queue;
	std::vector<std::thread> threads;

	closure pop();
	void push(closure &&);
	void worker() noexcept;
}

decltype(ircd::ctx::ole::thread_max)
ircd::ctx::ole::thread_max
{
	{ "name",     "ircd.ctx.ole.thread.max"  },
	{ "default",  int64_t(1)                 },
};

ircd::ctx::ole::init::init()
{
	assert(threads.empty());
	termination = false;
}

ircd::ctx::ole::init::~init()
noexcept
{
	std::unique_lock<decltype(mutex)> lock(mutex);
	termination = true;
	cond.notify_all();
	cond.wait(lock, []
	{
		return threads.empty();
	});
}

void
ircd::ctx::ole::offload(const std::function<void ()> &func)
{
	bool done(false);
	auto *const context(current);
	const auto kick([&context, &done]
	{
		done = true;
		notify(*context);
	});

	std::exception_ptr eptr;
	auto closure([&func, &eptr, &context, &kick]
	() noexcept
	{
		try
		{
			func();
		}
		catch(...)
		{
			eptr = std::current_exception();
		}

		// To wake the context on the IRCd thread we give it the kick
		signal(*context, kick);
	});

	// interrupt(ctx) is suppressed while this context has offloaded some work
	// to another thread. This context must stay right here and not disappear
	// until the other thread signals back. Note that the destructor is
	// capable of throwing an interrupt that was received during this scope.
	const uninterruptible uninterruptible;

	push(std::move(closure)); do
	{
		wait();
	}
	while(!done);

	// Don't throw any exception if there is a pending interrupt for this ctx.
	// Two exceptions will be thrown in that case and if there's an interrupt
	// we don't care about eptr anyway.
	if(eptr && likely(!interruption_requested()))
		std::rethrow_exception(eptr);
}

void
ircd::ctx::ole::push(closure &&func)
{
	if(unlikely(threads.size() < size_t(thread_max)))
		threads.emplace_back(&worker);

	const std::lock_guard<decltype(mutex)> lock(mutex);
	queue.emplace_back(std::move(func));
	cond.notify_all();
}

void
ircd::ctx::ole::worker()
noexcept try
{
	while(1)
	{
		const auto func(pop());
		func();
	}
}
catch(const interrupted &)
{
	std::unique_lock<decltype(mutex)> lock(mutex);
	const auto it(std::find_if(begin(threads), end(threads), []
	(const auto &thread)
	{
		return thread.get_id() == std::this_thread::get_id();
	}));

	assert(it != end(threads));
	auto &this_thread(*it);
	this_thread.detach();
	threads.erase(it);
	cond.notify_all();
}

ircd::ctx::ole::closure
ircd::ctx::ole::pop()
{
	std::unique_lock<decltype(mutex)> lock(mutex);
	cond.wait(lock, []
	{
		if(!queue.empty())
			return true;

		if(unlikely(termination))
			throw interrupted{};

		return false;
	});

	auto c(std::move(queue.front()));
	queue.pop_front();
	return std::move(c);
}

///////////////////////////////////////////////////////////////////////////////
//
// ctx/promise.h
//

//
// promise<void>
//

void
ircd::ctx::promise<void>::set_value()
{
	check_pending();
	make_ready();
}

ircd::ctx::shared_state<void> &
ircd::ctx::promise<void>::state()
{
	return promise_base::state<void>();
}

const ircd::ctx::shared_state<void> &
ircd::ctx::promise<void>::state()
const
{
	return promise_base::state<void>();
}

//
// promise_base::promise_base
//

ircd::ctx::promise_base::promise_base(promise_base &&o)
noexcept
:st{std::move(o.st)}
,next{std::move(o.next)}
{
	if(st)
	{
		update(*this, o);
		o.st = nullptr;
	}
}

ircd::ctx::promise_base::promise_base(const promise_base &o)
:st{o.st}
,next{nullptr}
{
	append(*this, const_cast<promise_base &>(o));
}

ircd::ctx::promise_base &
ircd::ctx::promise_base::operator=(promise_base &&o)
noexcept
{
	this->~promise_base();
	st = std::move(o.st);
	next = std::move(o.next);
	if(!st)
		return *this;

	update(*this, o);
	o.st = nullptr;
	return *this;
}

ircd::ctx::promise_base::~promise_base()
noexcept
{
	if(!valid())
		return;

	if(refcount(state()) == 1)
		set_exception(std::make_exception_ptr(broken_promise()));
	else
		remove(state(), *this);
}

void
ircd::ctx::promise_base::set_exception(std::exception_ptr eptr)
{
	check_pending();
	state().eptr = std::move(eptr);
	make_ready();
}

void
ircd::ctx::promise_base::make_ready()
{
	auto &st(state());

	// First we have to chase the linked list of promises reachable
	// from this shared_state. invalidate() will null their pointer
	// to the shared_state indicating the promise was already satisfied.
	// This is done first because the set() to the READY writes to the
	// same union as the promise pointer (see shared_state.h).
	invalidate(st);

	// Now set the shared_state to READY. We know the location of the
	// shared state by saving it in this frame earlier, otherwise invalidate()
	// would have nulled it.
	set(st, future_state::READY);

	// Finally call the notify() routine which will tell the future the promise
	// was satisfied and the value/exception is ready for them. This call may
	// notify an ircd::ctx and/or post a function to the ircd::ios for a then()
	// callback etc.
	notify(st);

	// At this point the future should no longer be considered valid; no longer
	// referring to the shared_state.
	assert(!valid());
}

void
ircd::ctx::promise_base::check_pending()
const
{
	assert(valid());
	if(unlikely(!is(state(), future_state::PENDING)))
		throw promise_already_satisfied{};
}

bool
ircd::ctx::promise_base::operator!()
const
{
	return !valid();
}

ircd::ctx::promise_base::operator bool()
const
{
	return valid();
}

bool
ircd::ctx::promise_base::valid()
const
{
	return bool(st);
}

ircd::ctx::shared_state_base &
ircd::ctx::promise_base::state()
{
	assert(valid());
	return *st;
}

const ircd::ctx::shared_state_base &
ircd::ctx::promise_base::state()
const
{
	assert(valid());
	return *st;
}

/// Internal semantics; chases the linked list of promises and adds a reference
/// to a new copy at the end (for copy semantic).
void
ircd::ctx::promise_base::append(promise_base &new_,
                                promise_base &old)
{
	if(!old.next)
	{
		old.next = &new_;
		return;
	}

	promise_base *next{old.next};
	for(; next->next; next = next->next);
	next->next = &new_;
}

/// Internal semantics; updates the location of a promise within the linked
/// list of related promises (for move semantic).
void
ircd::ctx::promise_base::update(promise_base &new_,
                                promise_base &old)
{
	assert(old.st);
	auto &st{*old.st};
	if(!is(st, future_state::PENDING))
		return;

	if(st.p == &old)
	{
		st.p = &new_;
		return;
	}

	promise_base *last{st.p};
	for(promise_base *next{last->next}; next; last = next, next = last->next)
		if(next == &old)
		{
			last->next = &new_;
			break;
		}
}

/// Internal semantics; removes the promise from the linked list of promises.
/// Because the linked list of promises is a forward singly-linked list this
/// operation requires a reference to the list's head in shared_state_base
/// (for dtor semantic).
void
ircd::ctx::promise_base::remove(shared_state_base &st,
                                promise_base &p)
{
	if(!is(st, future_state::PENDING))
		return;

	if(st.p == &p)
	{
		st.p = p.next;
		return;
	}

	promise_base *last{st.p};
	for(promise_base *next{last->next}; next; last = next, next = last->next)
		if(next == &p)
		{
			last->next = p.next;
			break;
		}
}

///////////////////////////////////////////////////////////////////////////////
//
// ctx/shared_shared.h
//

/// Internal use
void
ircd::ctx::notify(shared_state_base &st)
{
	if(!st.then)
	{
		st.cond.notify_all();
		return;
	}

	if(!current)
	{
		st.cond.notify_all();
		assert(bool(st.then));
		st.then(st);
		return;
	}

	const stack_usage_assertion sua;
	st.cond.notify_all();
	assert(bool(st.then));
	st.then(st);
}

/// Internal use; chases the linked list of promises starting from the head
/// in the shared_state and invalidates all of their references to the shared
/// state. This will cause the promise to no longer be valid().
///
void
ircd::ctx::invalidate(shared_state_base &st)
{
	if(is(st, future_state::PENDING))
		for(promise_base *p{st.p}; p; p = p->next)
			p->st = nullptr;
}

/// Internal use; chases the linked list of promises starting from the head in
/// the shared_state and updates the location of the shared_state within each
/// promise. This is used to tell the promises when the shared_state itself
/// has relocated.
///
void
ircd::ctx::update(shared_state_base &st)
{
	if(is(st, future_state::PENDING))
		for(promise_base *p{st.p}; p; p = p->next)
			p->st = &st;
}

/// Internal use; returns the number of copies of the promise reachable from
/// the linked list headed by the shared state. This is used to indicate when
/// the last copy has destructed which may result in a broken_promise exception
/// being sent to the future.
size_t
ircd::ctx::refcount(const shared_state_base &st)
{
	size_t ret{0};
	if(is(st, future_state::PENDING))
		for(const promise_base *p{st.p}; p; p = p->next)
			++ret;

	return ret;
}

/// Internal use; sets the state indicator within the shared_state object. Take
/// special note that this data is unionized. Setting a state here will clobber
/// the shared_state's reference to its promise.
void
ircd::ctx::set(shared_state_base &st,
               const future_state &state)
{
	switch(state)
	{
		case future_state::INVALID:  assert(0);  return;
		case future_state::PENDING:  assert(0);  return;
		case future_state::OBSERVED:
		case future_state::READY:
		case future_state::RETRIEVED:
		default:
			st.st = state;
			return;
	}
}

/// Internal; check if the current state is something; safe but unnecessary
/// for public use. Take special note here that the state value is unionized.
///
/// A PENDING state returned here does not mean the state contains the
/// enumerated PENDING value itself, but instead contains a valid pointer
/// to a promise.
///
/// An INVALID state shares a zero/null value in the unionized data.
bool
ircd::ctx::is(const shared_state_base &st,
              const future_state &state_)
{
	switch(st.st)
	{
		case future_state::READY:
		case future_state::OBSERVED:
		case future_state::RETRIEVED:
			return state_ == st.st;

		default: switch(state_)
		{
			case future_state::INVALID:
				return st.p == nullptr;

			case future_state::PENDING:
				return uintptr_t(st.p) >= 0x1000;

			default:
				return false;
		}
	}
}

/// Internal; get the current state of the shared_state; safe but unnecessary
/// for public use.
///
/// NOTE: This operates over a union of a pointer and an enum class. The
/// way we determine whether the data is a pointer or an enum value is
/// with a test of the value being >= the system's page size. This assumes
/// the system does not use the first page of a process's address space
/// to fault on null pointer dereference. This assumption may not hold on
/// all systems or in all environments.
///
/// Alternatively, we can switch this to checking whether the value is simply
/// above the few low-numbered enum values.
ircd::ctx::future_state
ircd::ctx::state(const shared_state_base &st)
{
	return uintptr_t(st.p) >= ircd::info::page_size?
		future_state::PENDING:
		st.st;
}

//
// shared_state_base::shared_state_base
//

ircd::ctx::shared_state_base::shared_state_base(promise_base &p)
:p{&p}
{
}

// Linkage
ircd::ctx::shared_state_base::~shared_state_base()
noexcept
{
	then = {};
}

///////////////////////////////////////////////////////////////////////////////
//
// ctx_list.h
//

void
ircd::ctx::list::remove(ctx *const &c)
{
	assert(c);

	if(c == head)
	{
		pop_front();
		return;
	}

	if(c == tail)
	{
		pop_back();
		return;
	}

	assert(next(c) && prev(c));
	prev(next(c)) = prev(c);
	next(prev(c)) = next(c);
	next(c) = nullptr;
	prev(c) = nullptr;
}

ircd::ctx::ctx *
ircd::ctx::list::pop_back()
{
	const auto tail
	{
		this->tail
	};

	if(!tail)
	{
		assert(!head);
		return tail;
	}

	assert(head);
	assert(!next(tail));
	if(!prev(tail))
	{
		this->head = nullptr;
		this->tail = nullptr;
	} else {
		assert(next(prev(tail)) == tail);
		next(prev(tail)) = nullptr;
		this->tail = prev(tail);
	}

	prev(tail) = nullptr;
	next(tail) = nullptr;
	return tail;
}

ircd::ctx::ctx *
ircd::ctx::list::pop_front()
{
	const auto head
	{
		this->head
	};

	if(!head)
	{
		assert(!tail);
		return head;
	}

	assert(tail);
	assert(!prev(head));
	if(!next(head))
	{
		this->head = nullptr;
		this->tail = nullptr;
	} else {
		assert(prev(next(head)) == head);
		prev(next(head)) = nullptr;
		this->head = next(head);
	}

	prev(head) = nullptr;
	next(head) = nullptr;
	return head;
}

void
ircd::ctx::list::push_front(ctx *const &c)
{
	assert(next(c) == nullptr);
	assert(prev(c) == nullptr);

	if(!head)
	{
		assert(!tail);
		head = c;
		tail = c;
		return;
	}

	assert(prev(head) == nullptr);
	prev(head) = c;
	next(c) = head;
	head = c;
}

void
ircd::ctx::list::push_back(ctx *const &c)
{
	assert(next(c) == nullptr);
	assert(prev(c) == nullptr);

	if(!tail)
	{
		assert(!head);
		head = c;
		tail = c;
		return;
	}

	assert(next(tail) == nullptr);
	next(tail) = c;
	prev(c) = tail;
	tail = c;
}

void
ircd::ctx::list::rfor_each(const std::function<void (ctx &)> &closure)
{
	for(ctx *tail{this->tail}; tail; tail = prev(tail))
		closure(*tail);
}

void
ircd::ctx::list::rfor_each(const std::function<void (const ctx &)> &closure)
const
{
	for(const ctx *tail{this->tail}; tail; tail = prev(tail))
		closure(*tail);
}

bool
ircd::ctx::list::rfor_each(const std::function<bool (ctx &)> &closure)
{
	for(ctx *tail{this->tail}; tail; tail = prev(tail))
		if(!closure(*tail))
			return false;

	return true;
}

bool
ircd::ctx::list::rfor_each(const std::function<bool (const ctx &)> &closure)
const
{
	for(const ctx *tail{this->tail}; tail; tail = prev(tail))
		if(!closure(*tail))
			return false;

	return true;
}

void
ircd::ctx::list::for_each(const std::function<void (ctx &)> &closure)
{
	for(ctx *head{this->head}; head; head = next(head))
		closure(*head);
}

void
ircd::ctx::list::for_each(const std::function<void (const ctx &)> &closure)
const
{
	for(const ctx *head{this->head}; head; head = next(head))
		closure(*head);
}

bool
ircd::ctx::list::for_each(const std::function<bool (ctx &)> &closure)
{
	for(ctx *head{this->head}; head; head = next(head))
		if(!closure(*head))
			return false;

	return true;
}

bool
ircd::ctx::list::for_each(const std::function<bool (const ctx &)> &closure)
const
{
	for(const ctx *head{this->head}; head; head = next(head))
		if(!closure(*head))
			return false;

	return true;
}

ircd::ctx::ctx *&
ircd::ctx::list::prev(ctx *const &c)
{
	assert(c);
	return c->node.prev;
}

ircd::ctx::ctx *&
ircd::ctx::list::next(ctx *const &c)
{
	assert(c);
	return c->node.next;
}

const ircd::ctx::ctx *
ircd::ctx::list::prev(const ctx *const &c)
{
	assert(c);
	return c->node.prev;
}

const ircd::ctx::ctx *
ircd::ctx::list::next(const ctx *const &c)
{
	assert(c);
	return c->node.next;
}
