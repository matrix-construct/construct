// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include "ctx.h"

/// Dedicated log facility for the ircd::ctx subsystem.
decltype(ircd::ctx::log)
ircd::ctx::log
{
	"ctx"
};

//
// ctx::ctx (internal)
//

/// Allocator instance for the ctx instance_list. This allocator will place
/// the std::list nodes in the ctx struct itself.
template<>
decltype(ircd::util::instance_list<ircd::ctx::ctx>::allocator)
ircd::util::instance_list<ircd::ctx::ctx>::allocator
{};

/// Instance list linkage for the list of all ctx instances. All ctxs can be
/// iterated through this list. The allocator makes the overhead of this list
/// negligible.
template<>
decltype(ircd::util::instance_list<ircd::ctx::ctx>::list)
ircd::util::instance_list<ircd::ctx::ctx>::list
{
	allocator
};

/// Monotonic ctx id counter state. This counter is incremented for each
/// newly created context.
decltype(ircd::ctx::ctx::id_ctr)
ircd::ctx::ctx::id_ctr
{
	0
};

/// This is a pseudo ircd::ios descriptor. We want to account for a ctx's
/// execution slice in the ircd::ios handler list. This posits the entire
/// ircd::ctx system as one ircd::ios handler type among all the others.
/// At this time it is unclear how to hook a context's execution slice in the ircd::ios system.
decltype(ircd::ctx::ctx::ios_desc)
ircd::ctx::ctx::ios_desc
{
	"ircd.ctx.ctx"
};

/// This is a pseudo ircd::ios handler. See ios_desc
decltype(ircd::ctx::ctx::ios_handler)
ircd::ctx::ctx::ios_handler
{
	&ios_desc
};

/// Points to the next context to spawn (internal use)
[[gnu::visibility("internal")]]
decltype(ircd::ctx::ctx::spawning)
ircd::ctx::ctx::spawning;

/// Used to notify of context completion
[[gnu::visibility("hidden")]]
decltype(ircd::ctx::ctx::adjoindre)
ircd::ctx::ctx::adjoindre;

/// Internal context struct ctor
ircd::ctx::ctx::ctx(const string_view &name,
                    const ircd::ctx::stack &stack,
                    const context::flags &flags)
:flags
{
	flags
}
,alarm
{
	ios::get()
}
,stack
{
	stack
}
{
	strlcpy(this->name, name);
}

ircd::ctx::ctx::~ctx()
noexcept
{
	assert(yc == nullptr); // Check that the context isn't active.
}

/// Internal wrapper for asio::spawn; never call directly.
void
IRCD_CTX_STACK_PROTECT
ircd::ctx::ctx::spawn(context::function func)
{
	const boost::coroutines::attributes attrs
	{
		// Pass the requested stack size
		stack.max,

		// We ensure stack unwinding and cleanup out here instead.
		boost::coroutines::no_stack_unwind,
	};

	const scope_restore spawning
	{
		ircd::ctx::ctx::spawning, this
	};

	auto bound
	{
		std::bind(&ctx::operator(), this, ph::_1, std::move(func))
	};

	const auto parent_context
	{
		ircd::ctx::current
	};

	const auto parent_handler
	{
		ircd::ios::handler::current
	};

	assert(!parent_context && parent_handler); try
	{
		assert(!ircd::ctx::current && ios::handler::current);
		ios::handler::leave(parent_handler);

		assert(!ircd::ctx::current && !ios::handler::current);
		boost::asio::spawn(ios::get(), ios::handle(ios_desc, std::move(bound)), attrs);

		assert(!ircd::ctx::current && !ios::handler::current);
		ios::handler::enter(parent_handler);
	}
	catch(...)
	{
		assert(!ircd::ctx::current && !ios::handler::current);
		ios::handler::enter(parent_handler);

		assert(!ircd::ctx::current && ios::handler::current == parent_handler);
		throw;
	}

	assert(!ircd::ctx::current && ios::handler::current == parent_handler);
}

/// Base frame for a context.
///
/// This function is the first thing executed on the new context's stack
/// and calls the user's function.
void
IRCD_CTX_STACK_PROTECT
ircd::ctx::ctx::operator()(boost::asio::yield_context yc,
                           const std::function<void ()> func)
noexcept try
{
	assert(!ircd::ctx::current);
	ircd::ctx::current = this;
	this->yc = &yc;
	notes = 1;
	stack.base = uintptr_t(__builtin_frame_address(0));
	const unwind atexit{[this]
	{
		adjoindre.notify_all();
		stack.at = 0;
		notes = 0;
		this->yc = nullptr;
		ircd::ctx::current = nullptr;
		if(flags & context::DETACH && !std::uncaught_exceptions())
			delete this;
	}};

	// Check for a precocious interrupt
	interruption_point();

	// Mark the point of context entry only after the interrupt check. If the
	// context was interrupted without ever entering (which makes the above
	// check throw) we never record any execution slice or increment the epoch
	// counter for it. This can allow a parent context to assume application
	// state remains unmodified by the aborted context.
	mark(prof::event::ENTER);
	const unwind leaver{[this]
	{
		mark(prof::event::LEAVE);
	}};

	// Call the user's function.
	func();

	assert(!std::uncaught_exceptions());
}
catch(const ircd::ctx::interrupted &)
{
	assert(!std::uncaught_exceptions());
	if(flags & context::DETACH)
		delete this;
}
catch(const ircd::ctx::terminated &)
{
	assert(!std::uncaught_exceptions());
	if(flags & context::DETACH)
		delete this;
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "ctx('%s' id:%u): unhandled: %s",
		name,
		id,
		e.what()
	};

	assert(!std::uncaught_exceptions());
	if(flags & context::DETACH)
		delete this;
}

/// Direct context switch to this context.
///
/// This currently doesn't work yet because the suspension state of this
/// context has to be ready to be jumped to and that isn't implemented yet.
void
IRCD_CTX_STACK_PROTECT
ircd::ctx::ctx::jump()
{
	assert(this->yc);
	assert(current != this);                  // can't jump to self

	auto &yc(*this->yc);
	auto &target(*yc.coro_.lock());

	// Jump from the currently running context (source) to *this (target)
	// with continuation of source after target
	current->notes = 0; // Unconditionally cleared here
	continuation
	{
		continuation::false_predicate, continuation::noop_interruptor, [&target]
		(auto &yield) noexcept
		{
			target();
		}
	};

	assert(current != this);
	assert(current->notes == 1); // notes = 1; set by continuation dtor on wakeup
}

/// Yield (suspend) this context until notified.
///
/// This context must be currently running otherwise bad things. Returns false
/// if the context was notified before actually suspending; the note is then
/// considered handled an another attempt to `wait()` can be made. Returns true
/// if the context suspended and was notified. When a context wakes up the
/// note counter is reset.
[[gnu::hot]]
bool
IRCD_CTX_STACK_PROTECT
ircd::ctx::ctx::wait()
{
	namespace errc = boost::system::errc;

	assert(this->yc);
	assert(current == this);
	assert(notes == 1);

	// Clear the notification counter.
	notes = 0;

	// This is currently a dummy predicate; this is where we can take the
	// user's real wakeup condition (i.e from a ctx::dock) and use it with
	// an internal scheduler.
	const predicate &predicate{[this]()
	noexcept
	{
		return notes > 0;
	}};

	// An interrupt invokes this closure to force the alarm to return.
	const interruptor &interruptor{[this]
	(ctx *const &interruptor)
	noexcept
	{
		wake();
	}};

	// The construction of the arguments to the call on this stack comprise
	// our final control before the context switch. The destruction of the
	// arguments comprise the initial control after the context switch.
	boost::system::error_code ec; continuation
	{
		predicate, interruptor, [this, &ec]
		(auto &yield)
		noexcept
		{
			alarm.async_wait(yield[ec]);
		}
	};

	assert(ec == errc::operation_canceled || ec == errc::success);
	assert(current == this);
	assert(notes == 1);  // notes = 1; set by continuation dtor on wakeup

	return true;
}

/// Notifies this context to resume (wake up from waiting).
///
/// Returns true if this note was the first note received by this context
/// while it's been suspended or false if it's already been notified.
bool
ircd::ctx::ctx::note()
noexcept
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
noexcept try
{
	if constexpr(ios::profile::logging)
	{
		assert(ios_desc.stats);
		log::logf
		{
			ios::log, log::level::DEBUG,
			"QUEUE %5u %-30s [%11lu] ------[%9lu] q:%-4lu id:%-5u %-30s",
			ios_desc.id,
			trunc(ios_desc.name, 30),
			uint64_t(ios_desc.stats->calls),
			notes,
			uint64_t(ios_desc.stats->queued),
			id,
			name,
		};
	}

	alarm.cancel();
	return true;
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "ctx::wake(%p): %s", this, e.what()
	};

	return false;
}

/// Throws if this context has been flagged for interruption and clears
/// the flag.
[[gnu::hot]]
void
ircd::ctx::ctx::interruption_point()
{
	if(unlikely(interruption()))
	{
		if(termination_point(std::nothrow))
			throw terminated{};

		if(likely(interruption_point(std::nothrow)))
			throw interrupted
			{
				"ctx:%lu '%s'", id, name
			};
	}
}

/// Returns true if this context has been flagged for termination. Does not
/// clear the flag. Sets the NOINTERRUPT flag so the context cannot be further
// interrupted which simplifies the termination process.
[[gnu::hot]]
bool
ircd::ctx::ctx::termination_point(std::nothrow_t)
noexcept
{
	if(unlikely(flags & context::TERMINATED))
	{
		assert(~flags & context::NOINTERRUPT);
		flags |= context::NOINTERRUPT;
		mark(prof::event::TERMINATE);
		return true;
	}
	else return false;
}

/// Returns true if this context has been flagged for interruption and
/// clears the flag.
[[gnu::hot]]
bool
ircd::ctx::ctx::interruption_point(std::nothrow_t)
noexcept
{
	if(unlikely(flags & context::INTERRUPTED))
	{
		assert(~flags & context::NOINTERRUPT);
		flags &= ~context::INTERRUPTED;
		mark(prof::event::INTERRUPT);
		return true;
	}
	else return false;
}

/// True if this context has been flagged for interruption or termination
/// and interrupts are not blocked.
[[gnu::hot]]
bool
ircd::ctx::ctx::interruption()
const noexcept
{
	static const auto &flags
	{
		context::TERMINATED | context::INTERRUPTED
	};

	// Fast test-and-bail for the very likely case there is no interrupt.
	if(likely((this->flags & flags) == 0))
		return false;

	// The NOINTERRUPT flag works by pretending there is no interrupt flag
	// set and also does not clear the flag. This allows the interrupt
	// to remain pending until the uninterruptible section is complete.
	if(this->flags & context::NOINTERRUPT)
		return false;

	return true;
}

[[gnu::hot]]
bool
ircd::ctx::ctx::started()
const noexcept
{
	return stack.base != 0;
}

[[gnu::hot]]
bool
ircd::ctx::ctx::finished()
const noexcept
{
	return started() && yc == nullptr;
}

///////////////////////////////////////////////////////////////////////////////
//
// ctx/ctx.h
//

[[gnu::hot]]
const uint64_t &
ircd::ctx::epoch()
noexcept
{
	return ctx::ios_handler.epoch;
}

bool
ircd::ctx::for_each(const std::function<bool (ctx &)> &closure)
{
	for(auto &ctx : ctx::list)
		if(!closure(*ctx))
			return false;

	return true;
}

/// Yield to context `ctx`.
///
///
[[gnu::hot]]
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

	ctx.note();
}

/// Notifies `ctx` to wake up from another std::thread
void
ircd::ctx::notify(ctx &ctx,
                  threadsafe_t)
{
	signal(ctx, [&ctx]()
	noexcept
	{
		notify(ctx);
	});
}

/// Notifies `ctx` to wake up. This will enqueue the resumption, not jump
/// directly to `ctx`.
bool
ircd::ctx::notify(ctx &ctx)
noexcept
{
	return ctx.note();
}

namespace ircd::ctx
{
	[[gnu::visibility("hidden")]]
	extern ios::descriptor signal_desc;
}

decltype(ircd::ctx::signal_desc)
ircd::ctx::signal_desc
{
	"ircd.ctx.signal"
};

/// Executes `func` sometime between executions of `ctx` with thread-safety
/// so `func` and `ctx` are never executed concurrently no matter how many
/// threads the io_service has available to execute events on.
void
ircd::ctx::signal(ctx &ctx,
                  std::function<void ()> func)
{
	ios::dispatch
	{
		signal_desc, ios::defer, std::move(func)
	};
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
	if(likely(&ctx != current && ctx.cont != nullptr))
		(*ctx.cont->intr)(current);
}

/// Marks `ctx` for interruption and enqueues it for resumption to receive the
/// interrupt which will be an exception coming out of the point where the
/// `ctx` was yielding.
///
/// NOTE: If the IRCd run::level is QUIT, an interrupt() becomes a terminate().
void
ircd::ctx::interrupt(ctx &ctx)
{
	if(unlikely(run::level == run::level::QUIT))
		return terminate(ctx);

	if(finished(ctx))
		return;

	if(interruption(ctx))
		return;

	ctx.flags |= context::INTERRUPTED;
	if(likely(&ctx != current && ctx.cont != nullptr))
		(*ctx.cont->intr)(current);
}

void
ircd::ctx::name(ctx &ctx,
                const string_view &name)
noexcept
{
	strlcpy(ctx.name, name);
}

int8_t
ircd::ctx::nice(ctx &ctx,
                const int8_t &val)
noexcept
{
	ctx.nice = val;
	return ctx.nice;
}

int8_t
ircd::ctx::ionice(ctx &ctx,
                  const int8_t &val)
noexcept
{
	ctx.ionice = val;
	return ctx.ionice;
}

/// Returns writable reference to the flags of ctx
[[gnu::hot]]
uint32_t &
ircd::ctx::flags(ctx &ctx)
noexcept
{
	return ctx.flags;
}

/// !running() && notes > 0
[[gnu::hot]]
bool
ircd::ctx::queued(const ctx &ctx)
noexcept
{
	return !running(ctx) && notes(ctx) > 0;
}

/// started() && !finished() && !running
[[gnu::hot]]
bool
ircd::ctx::waiting(const ctx &ctx)
noexcept
{
	return started(ctx) && !finished(ctx) && !running(ctx);
}

/// Indicates if `ctx` is the current ctx
[[gnu::hot]]
bool
ircd::ctx::running(const ctx &ctx)
noexcept
{
	return &ctx == current;
}

/// Indicates if `ctx` was ever jumped to
[[gnu::hot]]
bool
ircd::ctx::started(const ctx &ctx)
noexcept
{
	return ctx.started();
}

/// Indicates if the base frame for `ctx` returned
[[gnu::hot]]
bool
ircd::ctx::finished(const ctx &ctx)
noexcept
{
	return ctx.finished();
}

/// Returns the IO priority nice-value
[[gnu::hot]]
const int8_t &
ircd::ctx::ionice(const ctx &ctx)
noexcept
{
	return ctx.ionice;
}

/// Returns the context scheduling priority nice-value
[[gnu::hot]]
const int8_t &
ircd::ctx::nice(const ctx &ctx)
noexcept
{
	return ctx.nice;
}

/// Returns the notification count for `ctx`
[[gnu::hot]]
const int32_t &
ircd::ctx::notes(const ctx &ctx)
noexcept
{
	return ctx.notes;
}

/// Returns reference to the flags of ctx
[[gnu::hot]]
const uint32_t &
ircd::ctx::flags(const ctx &ctx)
noexcept
{
	return ctx.flags;
}

/// Returns the developer's optional name literal for `ctx`
[[gnu::hot]]
ircd::string_view
ircd::ctx::name(const ctx &ctx)
noexcept
{
	return ctx.name;
}

/// Returns a reference to unique ID for `ctx` (which will go away with `ctx`)
[[gnu::hot]]
const uint64_t &
ircd::ctx::id(const ctx &ctx)
noexcept
{
	return ctx.id;
}

///////////////////////////////////////////////////////////////////////////////
//
// ctx/this_ctx.h
//

decltype(ircd::ctx::this_ctx::courtesy_yield_desc)
ircd::ctx::this_ctx::courtesy_yield_desc
{
	"ircd.ctx.courtesy_yield",
	nullptr,
	nullptr,
	true,
};

// set by the continuation object and the base frame.
thread_local
ircd::ctx::ctx *
ircd::ctx::current;

/// Yield the currently running context until `time_point` ignoring notes
void
ircd::ctx::this_ctx::sleep_until(const system_point &tp)
{
	while(!wait_until(tp, std::nothrow));
}

/// Yield the currently running context for `duration` or until notified.
///
/// Returns the duration remaining if notified, or <= 0 if suspended for
/// the full duration, or unchanged if no suspend ever took place.
ircd::microseconds
ircd::ctx::this_ctx::wait(const microseconds &duration,
                          const std::nothrow_t &)
{
	const boost::posix_time::microseconds ptime_duration
	{
		duration.count()
	};

	auto &c(cur());
	c.alarm.expires_from_now(ptime_duration);
	c.wait(); // now you're yielding with portals
	const auto &ret
	{
		c.alarm.expires_from_now()
	};

	// return remaining duration.
	// this is > 0 if notified
	// this is unchanged if a note prevented any wait at all
	return microseconds(ret.total_microseconds());
}

/// Yield the currently running context until notified or `time_point`.
///
/// Returns true if this function returned because `time_point` was hit or
/// false because this context was notified.
bool
ircd::ctx::this_ctx::wait_until(const system_point &tp,
                                const std::nothrow_t &)
{
	const auto &diff
	{
		tp - now<system_point>()
	};

	const boost::posix_time::microseconds duration
	{
		duration_cast<microseconds>(diff).count()
	};

	const auto &expires_at
	{
		boost::posix_time::microsec_clock::universal_time() + duration
	};

	auto &c(cur());
	c.alarm.expires_at(expires_at);
	c.wait(); // now you're yielding with portals
	const auto &ret
	{
		c.alarm.expires_from_now()
	};

	return ret <= boost::posix_time::microseconds(0);
}

/// Yield the currently running context until notified.
[[gnu::hot]]
void
ircd::ctx::this_ctx::wait()
{
	auto &c(cur());
	c.alarm.expires_at(boost::posix_time::pos_infin);
	c.wait(); // now you're yielding with portals
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
ircd::ctx::this_ctx::interruption_point()
{
	// Asserting to know if this call is useless as it's being made in
	// an uninterruptible scope anyway. It's okay to relax this assertion.
	//assert(interruptible());
	return cur().interruption_point();
}

/// Returns unique ID of currently running context
[[gnu::hot]]
const uint64_t &
ircd::ctx::this_ctx::id()
noexcept
{
	static const uint64_t zero{0};
	return current? id(cur()) : zero;
}

//
// critical_assertion
//

namespace ircd::ctx
{
	extern thread_local bool critical_asserted;
}

decltype(ircd::ctx::critical_asserted)
thread_local
ircd::ctx::critical_asserted;

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
		throw panic
		{
			"%lu '%s' :Illegal context switch", id(), name()
		};
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
// ctx/slice_usage_warning.h
//

#ifdef RB_DEBUG
ircd::ctx::this_ctx::slice_usage_warning::slice_usage_warning(const string_view &fmt,
                                                              va_rtti &&ap)
:fmt
{
	fmt
}
,ap
{
	std::move(ap)
}
,epoch
{
	current?
		ircd::ctx::epoch(cur()):
		ircd::ctx::epoch()
}
,start
{
	// Set the start value to the total number of cycles accrued by this
	// context including the current time slice.
	!current?
		prof::cycles():
	~cur().flags & context::SLICE_EXEMPT?
		prof::get(cur(), prof::event::CYCLES) + prof::cur_slice_cycles():
		0
}
{
}
#endif

#ifdef RB_DEBUG
ircd::ctx::this_ctx::slice_usage_warning::~slice_usage_warning()
noexcept
{
	if(current && cur().flags & context::SLICE_EXEMPT)
		return;

	// Set the final value by first adding the total number of cycles ever
	// for this context to the current time slice. Then subtract the start
	// sample. This way we're only counting the execution time of this context
	// and not counting any time while it's yielding. A simple difference of
	// two rdtsc() samples would be insufficient.

	const auto stop
	{
		current?
			prof::get(cur(), prof::event::CYCLES) + prof::cur_slice_cycles():
			prof::cycles()
	};

	assert(stop >= start);
	const auto total(stop - start);
	if(likely(!prof::slice_exceeded_warning(total)))
		return;

	const auto span
	{
		current?
			ircd::ctx::epoch(cur()) - this->epoch:
			ircd::ctx::epoch() - this->epoch
	};

	thread_local char buf[256];
	const string_view reason{fmt::vsprintf
	{
		buf, fmt, ap
	}};

	const ulong &threshold{prof::settings::slice_warning};
	log::dwarning
	{
		prof::watchdog, "timeslice excessive; lim:%lu this:%lu pct:%.2lf span:%lu :%s",
		threshold,
		total,
		(double(total) / double(threshold)) * 100.0,
		span,
		reason
	};
}
#endif

///////////////////////////////////////////////////////////////////////////////
//
// ctx/continuation.h
//

decltype(ircd::ctx::continuation::true_predicate)
ircd::ctx::continuation::asio_predicate{[]()
noexcept -> bool
{
	return false;
}};

decltype(ircd::ctx::continuation::true_predicate)
ircd::ctx::continuation::true_predicate{[]()
noexcept -> bool
{
	return true;
}};

decltype(ircd::ctx::continuation::false_predicate)
ircd::ctx::continuation::false_predicate{[]()
noexcept -> bool
{
	return false;
}};

decltype(ircd::ctx::continuation::noop_interruptor)
ircd::ctx::continuation::noop_interruptor{[]
(ctx *const &interruptor)
noexcept -> void
{
	return;
}};

[[gnu::hot]]
void
ircd::ctx::continuation::leave()
noexcept
{
	assert(self != nullptr);
	assert(self->notes <= 1);

	// Check here if the developer has placed a critical assertion on the stack
	// but this yield is still occuring under its scope. That's bad.
	assert_critical();
	assert(!critical_asserted);

	// Confirming the uncaught exception count was saved and set to zero in the
	// initializer list.
	assert(!std::uncaught_exceptions());

	// Note: Construct an instance of ctx::exception_handler to enable yielding
	// in your catch block.
	//
	// GNU cxxabi uses a singly-linked forward list (aka the 'exception
	// stack') for pending exception activities. Due to this limitation we
	// cannot interleave _cxa_begin_catch() and __cxa_end_catch() by yielding
	// the ircd::ctx in an exception handler.
	assert(!std::current_exception());

	// Check that we saved a valid context reference to this object for later.
	assert(self->yc);

	// Point to this continuation instance (which is on the context's stack)
	// from the context's instance. This allows its features to be accessed
	// while the context is asleep (i.e interruptor and predicate functions).
	// NOTE that this pointer is not ever null'ed after being set here. It will
	// remain invalid once the context resumes. You know if this is a valid
	// pointer because the context is asleep; otherwise it's a trash value.
	self->cont = this;

	// Tell the profiler this is the point where the context has concluded
	// its execution run and is now yielding.
	mark(prof::event::YIELD);

	// Null the fundamental current context register as the last operation
	// during execution before yielding. When a context resumes it will
	// restore this register; otherwise it remains null for executions on
	// the program's main stack.
	ircd::ctx::current = nullptr;
}

[[gnu::hot]]
void
ircd::ctx::continuation::enter()
{
	// Restore the current context register.
	ircd::ctx::current = self;

	// Unconditionally reset the notes counter to 1 because we're awake now.
	self->notes = 1;

	// Restore exception state
	assert(std::uncaught_exceptions() == 0);
	exception_handler::uncaught_exceptions(uncaught_exceptions);

	// self->continuation is not null'ed here; it remains an invalid
	// pointer while the context is awake.

	// Tell the profiler this is the point where the context is now resuming.
	mark(prof::event::CONTINUE);

	// Check for an interrupt or termination that was sent while asleep.
	if(unlikely(self->interruption()))
	{
		self->interruption_point();
		__builtin_unreachable();
	}
}

ircd::ctx::continuation::operator
boost::asio::yield_context &()
noexcept
{
	assert(self);
	assert(self->yc);
	return *self->yc;
}

ircd::ctx::continuation::operator
const boost::asio::yield_context &()
const noexcept
{
	assert(self);
	assert(self->yc);
	return *self->yc;
}

///////////////////////////////////////////////////////////////////////////////
//
// ctx/context.h
//

namespace ircd::ctx
{
	[[gnu::visibility("hidden")]]
	extern ios::descriptor spawn_desc[3];
}

decltype(ircd::ctx::spawn_desc)
ircd::ctx::spawn_desc
{
	{ "ircd.ctx.spawn.post"      },
	{ "ircd.ctx.spawn.defer"     },
	{ "ircd.ctx.spawn.dispatch"  },
};

decltype(ircd::ctx::DEFAULT_STACK_SIZE)
ircd::ctx::DEFAULT_STACK_SIZE
{
	128_KiB
};

//
// context::context
//

// Linkage here for default construction because ctx is internal.
ircd::ctx::context::context()
{
}

ircd::ctx::context::context(const string_view &name,
                            const size_t &stack_size,
                            function func,
                            const flags &flags)
:context
{
	name, stack_size, flags, std::move(func)
}
{
}

ircd::ctx::context::context(const string_view &name,
                            const flags &flags,
                            function func)
:context
{
	name, DEFAULT_STACK_SIZE, flags, std::move(func)
}
{
}

ircd::ctx::context::context(const string_view &name,
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

ircd::ctx::context::context(const string_view &name,
                            const size_t &stack_sz,
                            const flags &flags,
                            function func)
:context
{
	name, mutable_buffer{nullptr, stack_sz}, flags, std::move(func)
}
{
}

ircd::ctx::context::context(const string_view &name,
                            const mutable_buffer &stack,
                            const flags &flags,
                            function func)
:c
{
	std::make_unique<ctx>
	(
		name,
		stack,
		!current? flags | POST : flags
	)
}
{
	auto spawn
	{
		std::bind(&ctx::spawn, c.get(), std::move(func))
	};

	// When the user passes the DETACH flag we want to release the unique_ptr
	// of the ctx if and only if that ctx is committed to freeing itself. Our
	// commitment ends at the 180 of this function. If no exception was thrown
	// we expect the context to be committed to entry. If the POST flag is
	// supplied and it gets lost in the asio queue it will not be entered, and
	// will not be able to free itself; that will leak.
	const unwind_nominal release{[this]
	{
		assert(c);
		if(c->flags & context::DETACH)
			this->detach();
	}};

	// Indicates to the profiler that this context is spawning a child.
	if(likely(ircd::ctx::current))
		mark(prof::event::SPAWN);

	// Branch to spawn via POST mechanism. This is an asynchronous method which
	// returns immediately so this context doesn't yield. The spawning occurs
	// sometime after this context next yields. This is the primary method to
	// spawn contexts. Note: This is the method to spawn contexts when this
	// parent is not itself a context as yielding is not possible anyway.
	assert(c->flags & POST || ircd::ctx::current);
	if(c->flags & POST)
		ios::dispatch
		{
			spawn_desc[0], ios::defer, std::move(spawn)
		};

	// (experimental) Branch to spawn via defer mechanism.
	else if(c->flags & DEFER)
		ios::dispatch
		{
			spawn_desc[1], ios::defer, ios::yield, std::move(spawn)
		};

	// Branch to spawn via dispatch mechanism. This context will yield while
	// the spawning takes place on this stack. This is the closest to a direct
	// context switch since we don't call spawn() directly from this frame
	// which allows the ctx/ios infrastructure to account for the context
	// switch. Note: This is also the default method when no flags are given
	// and this parent is another context.
	else if(c->flags & DISPATCH || (true))
		ios::dispatch
		{
			spawn_desc[2], ios::yield, std::move(spawn)
		};
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

		// When the WAIT_JOIN flag is given we wait for the context to
		// complete cooperatively before this destructs.
		if(~c->flags & context::WAIT_JOIN)
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
	ctx::adjoindre.wait([this]
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

const ircd::string_view &
ircd::ctx::name(const pool &pool)
{
	return pool.name;
}

decltype(ircd::ctx::pool::default_name)
ircd::ctx::pool::default_name
{
	"<unnamed pool>"
};

decltype(ircd::ctx::pool::default_opts)
ircd::ctx::pool::default_opts
{};

//
// pool::pool
//

ircd::ctx::pool::pool(const string_view &name,
                      const opts &opt)
:name{name}
,opt{&opt}
{
	// Can't spawn contexts when the ios isn't available. This may be the
	// case for some static instances of pool: initial_ctxs value is ignored.
	if(ircd::ios::available())
		add(this->opt->initial_ctxs);
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
ircd::ctx::pool::min(const size_t &num)
{
	if(size() < num)
		set(num);
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
	assert(opt);
	for(size_t i(0); i < num; ++i)
	{
		ctxs.emplace_back(name, opt->stack_size, context::POST, std::bind(&pool::main, this));

		assert(opt);
		ionice(ctxs.back(), opt->ionice);
		nice(ctxs.back(), opt->nice);
	}
}

void
ircd::ctx::pool::operator()(closure closure)
{
	assert(opt);
	if(!avail() && q.size() > size_t(opt->queue_max_soft) && opt->queue_max_dwarning)
		log::dwarning
		{
			log, "pool(%p '%s') ctx(%p): size:%zu active:%zu queue:%zu exceeded soft max:%zu",
			this,
			name,
			current,
			size(),
			active(),
			q.size(),
			opt->queue_max_soft
		};

	if(current && opt->queue_max_soft >= 0 && opt->queue_max_blocking)
		q_max.wait([this]
		{
			return !wouldblock();
		});

	if(unlikely(q.size() >= size_t(opt->queue_max_hard)))
		throw error
		{
			"pool(%p '%s') ctx(%p): size:%zu avail:%zu queue:%zu exceeded hard max:%zu",
			this,
			name,
			current,
			size(),
			avail(),
			q.size(),
			opt->queue_max_hard
		};

	q.push(std::move(closure));
}

bool
ircd::ctx::pool::wouldblock()
const
{
	if(q.size() < size_t(opt->queue_max_soft))
		return false;

	if(!opt->queue_max_soft && q.size() < avail())
		return false;

	return true;
}

/// Main execution loop for a pool.
void
ircd::ctx::pool::main()
noexcept try
{
	const scope_count running
	{
		this->running
	};

	q_max.notify();
	while(!termination(cur()))
		work();
}
catch(const interrupted &e)
{
//	log::debug
//	{
//		log, "pool(%p) ctx(%p): %s", this, &cur(), e.what()
//	};
}
catch(const terminated &e)
{
//	log::debug
//	{
//		log, "pool(%p) ctx(%p): terminated", this, &cur()
//	};
}

void
ircd::ctx::pool::work()
try
{
	const auto func
	{
		q.pop()
	};

	const scope_notify notify
	{
		q_max
	};

	const scope_count working
	{
		this->working
	};

	// Execute the user's function
	func();

	// Check for latent interruption to this ctx. If there's anything pending
	// it's best to get rid of it sooner rather than later.
	interruption_point();
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
		log, "pool(%p '%s') ctx(%p '%s' id:%u): unhandled: %s",
		this,
		name,
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
		log, "pool '%s' total: %zu avail: %zu queued: %zu active: %zu pending: %zu",
		pool.name,
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
	thread_local ticker _total;                // Totals kept for all contexts.

	static void check_stack();
	static void check_slice();
	static void slice_leave() noexcept;
	static void slice_enter() noexcept;

	static void handle_cur_yield();
	static void handle_cur_leave();
	static void handle_cur_continue() noexcept;
	static void handle_cur_enter() noexcept;

	static void inc_ticker(const event &e) noexcept;
}

decltype(ircd::ctx::prof::watchdog)
ircd::ctx::prof::watchdog
{
	"ctx.watchdog"
};

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

[[using gnu: flatten, always_inline]]
inline void
ircd::ctx::prof::mark(const event &e)
{
	inc_ticker(e);

	switch(e)
	{
		case event::ENTER:       handle_cur_enter();     break;
		case event::LEAVE:       handle_cur_leave();     break;
		case event::YIELD:       handle_cur_yield();     break;
		case event::CONTINUE:    handle_cur_continue();  break;
		default:                                         break;
	}
}

void
ircd::ctx::prof::inc_ticker(const event &e)
noexcept
{
	assert(uint8_t(e) < num_of<event>());

	// Increment the ticker for all contexts.
	_total.event[uint8_t(e)]++;

	// Increment the ticker for the context's instance
	static uint64_t dummy;
	uint64_t &ticker
	{
		current?
			current->profile.event[uint8_t(e)]:
			dummy
	};

	++ticker;
}

void
ircd::ctx::prof::handle_cur_enter()
noexcept
{
	slice_enter();
}

void
ircd::ctx::prof::handle_cur_continue()
noexcept
{
	slice_enter();
}

void
ircd::ctx::prof::handle_cur_leave()
{
	slice_leave();
	check_slice();
}

void
ircd::ctx::prof::handle_cur_yield()
{
	slice_leave();
	check_slice();
	check_stack();
}

void
ircd::ctx::prof::slice_enter()
noexcept
{
	ios::handler::enter(&ctx::ios_handler);
}

void
ircd::ctx::prof::slice_leave()
noexcept
{
	ios::handler::leave(&ctx::ios_handler);

	static constexpr auto pos
	{
		size_t(prof::event::CYCLES)
	};

	assert(ctx::ios_desc.stats);
	const uint64_t &last_slice
	{
		ctx::ios_desc.stats->slice_last
	};

	auto &c(cur());
	c.profile.event.at(pos) += last_slice;
	c.stack.at = stack_at_here();
	c.stack.peak = std::max(c.stack.at, c.stack.peak);

	_total.event.at(pos) += last_slice;
}

#ifndef NDEBUG
void
ircd::ctx::prof::check_slice()
{
	auto &c(cur());
	const auto &slice_exempt
	{
		c.flags & context::SLICE_EXEMPT
	};

	assert(ctx::ios_desc.stats);
	const uint64_t &last_slice
	{
		ctx::ios_desc.stats->slice_last
	};

	// Slice warning
	if(unlikely(slice_exceeded_warning(last_slice) && !slice_exempt))
		log::dwarning
		{
			watchdog, "timeslice excessive; lim:%lu last:%lu pct:%.2lf",
			ulong(settings::slice_warning),
			last_slice,
			((double(last_slice) / double(ulong(settings::slice_warning))) * 100.0)
		};

	// Slice assertion
	assert(!slice_exceeded_assertion(last_slice) || slice_exempt);

	// Slice interrupt
	if(unlikely(slice_exceeded_interrupt(last_slice) && !slice_exempt))
		throw interrupted
		{
			"[%s] context id:%lu watchdog interrupt; lim:%lu last:%lu total:%lu",
			name(c),
			id(c),
			ulong(settings::slice_interrupt),
			last_slice,
			cycles(c),
		};
}
#else
void
ircd::ctx::prof::check_slice()
{
}
#endif // NDEBUG

#ifndef NDEBUG
void
ircd::ctx::prof::check_stack()
{
	auto &c(cur());
	const auto &stack_exempt
	{
		c.flags & context::STACK_EXEMPT
	};

	const auto &stack_at
	{
		c.stack.at
	};

	// Stack warning
	if(unlikely(!stack_exempt && stack_exceeded_warning(stack_at)))
		log::dwarning
		{
			watchdog, "stack used %zu of %zu bytes",
			stack_at,
			c.stack.max
		};

	// Stack assertion
	assert(stack_exempt || !stack_exceeded_assertion(stack_at));
}
#else
void
ircd::ctx::prof::check_stack()
{
}
#endif // NDEBUG

bool
ircd::ctx::prof::stack_exceeded_assertion(const size_t &stack_at)
noexcept
{
	const auto &c(cur());
	const auto &stack_max(c.stack.max);
	const double &stack_usage_assertion(settings::stack_usage_assertion);
	return stack_usage_assertion > 0.0?
		stack_at >= c.stack.max * settings::stack_usage_assertion:
		false;
}

bool
ircd::ctx::prof::stack_exceeded_warning(const size_t &stack_at)
noexcept
{
	const auto &c(cur());
	const auto &stack_max(c.stack.max);
	const double &stack_usage_warning(settings::stack_usage_warning);
	return stack_usage_warning > 0.0?
		stack_at >= c.stack.max * stack_usage_warning:
		false;
}

bool
ircd::ctx::prof::slice_exceeded_interrupt(const ulong &cycles)
noexcept
{
	const ulong &threshold(settings::slice_interrupt);
	return threshold > 0 && cycles >= threshold;
}

bool
ircd::ctx::prof::slice_exceeded_assertion(const ulong &cycles)
noexcept
{
	const ulong &threshold(settings::slice_assertion);
	return threshold > 0 && cycles >= threshold;
}

bool
ircd::ctx::prof::slice_exceeded_warning(const ulong &cycles)
noexcept
{
	const ulong &threshold(settings::slice_warning);
	return threshold > 0 && cycles >= threshold;
}

[[gnu::hot]]
ulong
ircd::ctx::prof::cur_slice_start()
noexcept
{
	return ctx::ios_handler.ts;
}

[[gnu::hot]]
const uint64_t &
ircd::ctx::prof::get(const ctx &c,
                     const event &e)
{
	return get(c).event.at(uint8_t(e));
}

[[gnu::hot]]
const ircd::ctx::prof::ticker &
ircd::ctx::prof::get(const ctx &c)
noexcept
{
	return c.profile;
}

[[gnu::hot]]
const uint64_t &
ircd::ctx::prof::get(const event &e)
{
	return get().event.at(uint8_t(e));
}

[[gnu::hot]]
const ircd::ctx::prof::ticker &
ircd::ctx::prof::get()
noexcept
{
	return _total;
}

ircd::string_view
ircd::ctx::prof::reflect(const event &e)
{
	switch(e)
	{
		case event::SPAWN:       return "SPAWN";
		case event::JOIN:        return "JOIN";
		case event::JOINED:      return "JOINED";
		case event::ENTER:       return "ENTER";
		case event::LEAVE:       return "LEAVE";
		case event::YIELD:       return "YIELD";
		case event::CONTINUE:    return "CONTINUE";
		case event::INTERRUPT:   return "INTERRUPT";
		case event::TERMINATE:   return "TERMINATE";
		case event::CYCLES:      return "CYCLES";
		case event::_NUM_:       break;
	}

	return "?????";
}

///////////////////////////////////////////////////////////////////////////////
//
// ctx/promise.h
//

namespace ircd::ctx
{
	static void set_promises_state(shared_state_base &);
	static void invalidate_promises(shared_state_base &);
	static void append(shared_state_base &new_, shared_state_base &old);
	static void update(shared_state_base &new_, shared_state_base &old);
	static void remove(shared_state_base &);
	static void notify(shared_state_base &);

	static void set_futures_promise(promise_base &);
	static void invalidate_futures(promise_base &);
	static void append(promise_base &new_, promise_base &old);
	static void update(promise_base &new_, promise_base &old);
	static void remove(promise_base &);
}

//
// promise<void>
//

void
ircd::ctx::promise<void>::set_value()
{
	if(!valid())
		return;

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
	update(*this, o);
}

ircd::ctx::promise_base::promise_base(const promise_base &o)
:st{o.st}
,next{nullptr}
{
	append(*this, mutable_cast(o));
}

ircd::ctx::promise_base &
ircd::ctx::promise_base::operator=(promise_base &&o)
noexcept
{
	this->~promise_base();
	st = std::move(o.st);
	next = std::move(o.next);
	update(*this, o);
	return *this;
}

ircd::ctx::promise_base::~promise_base()
noexcept
{
	if(promise_base::refcount(*this) == 1)
		set_exception(make_exception_ptr<broken_promise>());

	remove();
}

void
ircd::ctx::promise_base::set_exception(std::exception_ptr eptr)
{
	if(!valid())
		return;

	check_pending();
	for(auto *st(shared_state_base::head(*this)); st; st = st->next)
		st->eptr = eptr;

	make_ready();
}

void
ircd::ctx::promise_base::remove()
{
	if(!valid())
		return;

	ircd::ctx::remove(*this);
	assert(!valid());
}

void
ircd::ctx::promise_base::make_ready()
{
	const critical_assertion ca;

	assert(valid());
	promise_base *p
	{
		promise_base::head(*this)
	};

	assert(p);
	shared_state_base *next
	{
		shared_state_base::head(*p)
	};

	// First we have to chase the linked list of promises reachable
	// from this shared_state. invalidate() will null their pointer
	// to the shared_state indicating the promise was already satisfied.
	// This is done first because the set() to the READY writes to the
	// same union as the promise pointer (see shared_state.h). Then
	// chase the linked lists of futures and make_ready() each one.
	assert(next);
	invalidate_promises(*next); do
	{
		// Now set the shared_state to READY. We know the location of the
		// shared state by saving it in this frame earlier, otherwise
		// invalidate_promises() would have nulled it.
		set(*next, future_state::READY);

		// Finally call the notify() routine which will tell the future the promise
		// was satisfied and the value/exception is ready for them. This call may
		// notify an ircd::ctx and/or post a function to the ircd::ios for a then()
		// callback etc.
		notify(*next);
	}
	while((next = next->next));

	// At this point the promise should no longer be considered valid; no longer
	// referring to the shared_state.
	this->st = nullptr;
	assert(!valid());
}

/// If no shared state anymore: refcount=0; otherwise the promise
/// list head from p.st->p resolves to at least &p which means refcount>=1
size_t
ircd::ctx::promise_base::refcount(const promise_base &p)
{
	const auto ret
	{
		p.st? refcount(*p.st): 0UL
	};

	assert((p.st && ret >= 1) || (!p.st && !ret));
	return ret;
}

/// Internal use; returns the number of copies of the promise reachable from
/// the linked list headed by the shared state. This is used to indicate when
/// the last copy has destructed which may result in a broken_promise exception
/// being sent to the future.
size_t
ircd::ctx::promise_base::refcount(const shared_state_base &st)
{
	size_t ret{0};
	if(!is(st, future_state::PENDING))
		return ret;

	for(const auto *next(head(st)); next; next = next->next)
		++ret;

	return ret;
}

ircd::ctx::promise_base *
ircd::ctx::promise_base::head(promise_base &p)
{
	return p.st && head(*p.st)?
		head(*p.st):
		std::addressof(p);
}

ircd::ctx::promise_base *
ircd::ctx::promise_base::head(shared_state_base &st)
{
	return is(st, future_state::PENDING)?
		st.p:
		nullptr;
}

const ircd::ctx::promise_base *
ircd::ctx::promise_base::head(const promise_base &p)
{
	return p.st && head(*p.st)?
		head(*p.st):
		std::addressof(p);
}

const ircd::ctx::promise_base *
ircd::ctx::promise_base::head(const shared_state_base &st)
{
	return is(st, future_state::PENDING)?
		st.p:
		nullptr;
}

//
// internal
//

/// Internal semantics; removes the promise from the linked list of promises.
void
ircd::ctx::remove(promise_base &p)
{
	promise_base *last
	{
		promise_base::head(p)
	};

	if(last == &p)
	{
		if(p.next)
			set_futures_promise(*p.next);
		else
			invalidate_futures(p);
	}
	else if(last)
		for(auto *next{last->next}; next; last = next, next = next->next)
			if(next == &p)
			{
				last->next = next->next;
				break;
			}

	p.st = nullptr;
	p.next = nullptr;
}

/// Internal semantics; updates the location of a promise within the linked
/// list of related promises (for move semantic).
void
ircd::ctx::update(promise_base &new_,
                  promise_base &old)
{
	new_.next = old.next;
	promise_base *last
	{
		promise_base::head(old)
	};

	if(last == &old)
		set_futures_promise(new_);

	else if(last)
		for(auto *next{last->next}; next; last = next, next = last->next)
			if(next == &old)
			{
				last->next = &new_;
				break;
			}

	old.st = nullptr;
	old.next = nullptr;
}

/// Internal semantics; chases the linked list of promises and adds a reference
/// to a new copy at the end (for copy semantic).
void
ircd::ctx::append(promise_base &new_,
                  promise_base &old)
{
	assert(new_.st);
	assert(old.st);
	if(!old.next)
	{
		old.next = &new_;
		return;
	}

	promise_base *next{old.next};
	for(; next->next; next = next->next);

	assert(!next->next);
	next->next = &new_;
}

void
ircd::ctx::set_futures_promise(promise_base &p)
{
	auto *next
	{
		shared_state_base::head(p)
	};

	for(; next; next = next->next)
	{
		assert(is(*next, future_state::PENDING));
		next->p = std::addressof(p);
	}
}

void
ircd::ctx::invalidate_futures(promise_base &p)
{
	auto *next
	{
		shared_state_base::head(p)
	};

	for(; next; next = next->next)
	{
		assert(is(*next, future_state::PENDING));
		next->p = nullptr;
	}
}

///////////////////////////////////////////////////////////////////////////////
//
// ctx/shared_shared.h
//

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
			st.p = nullptr;
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
noexcept
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
				return uintptr_t(st.p) >= ircd::info::page_size;

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
noexcept
{
	return uintptr_t(st.p) >= ircd::info::page_size?
		future_state::PENDING:
		st.st;
}

//
// shared_state_base::shared_state_base
//

ircd::ctx::shared_state_base::shared_state_base()
noexcept
{
}

ircd::ctx::shared_state_base::shared_state_base(already_t)
noexcept
{
	set(*this, future_state::READY);
}

ircd::ctx::shared_state_base::shared_state_base(promise_base &p)
{
	// assign the promise pointer in our new shared_state contained
	// in the future. If the promise already has a shared_state, that
	// means this is a shared future.
	this->p = promise_base::head(p);
	assert(!this->next);

	// Add this future (shared_state) to the end of the list of futures. Else
	// this is not a shared future, this is the head of the futures list told
	// to all shared promises.
	if(!p.st)
	{
		p.st = this;
		set_promises_state(*this);
	}
	else append(*this, *p.st);

	assert(p.valid());
	assert(is(*this, future_state::PENDING));
}

ircd::ctx::shared_state_base::shared_state_base(shared_state_base &&o)
noexcept
:cond{std::move(o.cond)}
,eptr{std::move(o.eptr)}
,then{std::move(o.then)}
,next{std::move(o.next)}
,p{std::move(o.p)}
{
	update(*this, o);
}

ircd::ctx::shared_state_base::shared_state_base(const shared_state_base &o)
:p{o.p}
{
	append(*this, mutable_cast(o));
}

ircd::ctx::shared_state_base &
ircd::ctx::shared_state_base::operator=(promise_base &p)
{
	this->~shared_state_base();
	new (this) shared_state_base{p};

	assert(p.valid());
	assert(is(*this, future_state::PENDING));
	return *this;
}

ircd::ctx::shared_state_base &
ircd::ctx::shared_state_base::operator=(shared_state_base &&o)
noexcept
{
	this->~shared_state_base();
	eptr = std::move(o.eptr);
	then = std::move(o.then);
	next = std::move(o.next);
	p = std::move(o.p);
	update(*this, o);
	return *this;
}

ircd::ctx::shared_state_base &
ircd::ctx::shared_state_base::operator=(const shared_state_base &o)
{
	this->~shared_state_base();
	eptr = o.eptr;
	then = o.then;
	p = o.p;
	append(*this, mutable_cast(o));
	return *this;
}

ircd::ctx::shared_state_base::~shared_state_base()
noexcept
{
	const auto refcount
	{
		shared_state_base::refcount(*this)
	};

	if(refcount == 1)
		invalidate_promises(*this);
	else if(refcount > 1)
		remove(*this);
}

//
// util
//

/// Count the number of associated futures
size_t
ircd::ctx::shared_state_base::refcount(const shared_state_base &st)
{
	size_t ret{0};
	if(!is(st, future_state::PENDING))
		return ret;

	for(const auto *next(head(st)); next; next = next->next)
		++ret;

	return ret;
}

/// Get the head of the futures from any associated promise
ircd::ctx::shared_state_base *
ircd::ctx::shared_state_base::head(promise_base &p)
{
	return p.st;
}

/// Get the head of the futures from any associated future
ircd::ctx::shared_state_base *
ircd::ctx::shared_state_base::head(shared_state_base &st)
{
	return is(st, future_state::PENDING) && head(*st.p)?
		head(*st.p):
		std::addressof(st);
}

/// Get the head of the futures from any associated promise
const ircd::ctx::shared_state_base *
ircd::ctx::shared_state_base::head(const promise_base &p)
{
	return p.st;
}

/// Get the head of the futures from any associated future
const ircd::ctx::shared_state_base *
ircd::ctx::shared_state_base::head(const shared_state_base &st)
{
	return is(st, future_state::PENDING) && head(*st.p)?
		head(*st.p):
		std::addressof(st);
}

//
// internal
//

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
		st.then = {};
		return;
	}

	const stack_usage_assertion sua;
	st.cond.notify_all();
	assert(bool(st.then));
	st.then(st);
	st.then = {};
}

/// Remove the future from the list of futures.
void
ircd::ctx::remove(shared_state_base &st)
{
	shared_state_base *last
	{
		shared_state_base::head(st)
	};

	assert(last);
	if(last == &st && is(st, future_state::PENDING))
	{
		if(last->next)
			set_promises_state(*last->next);
		else
			invalidate_promises(st);
	}
	else for(auto *next(last->next); next; last = next, next = next->next)
		if(next == &st)
		{
			last->next = next->next;
			break;
		}

	st.next = nullptr;
	st.p = nullptr;
}

/// Replace associated future old with new_. This is used to implement the
/// object move semantics.
void
ircd::ctx::update(shared_state_base &new_,
                  shared_state_base &old)
{
	shared_state_base *last
	{
		shared_state_base::head(old)
	};

	assert(last);
	if(last == &old && is(*last, future_state::PENDING))
		set_promises_state(new_);

	new_.next = old.next;
	for(auto *next{last->next}; next; last = next, next = last->next)
		if(next == &old)
		{
			last->next = &new_;
			break;
		}

	old.p = nullptr;
	old.next = nullptr;
}

/// Add a new future sharing the list
void
ircd::ctx::append(shared_state_base &new_,
                  shared_state_base &old)
{
	assert(!new_.next);
	assert(is(new_, future_state::PENDING));
	if(!old.next)
	{
		old.next = &new_;
		return;
	}

	shared_state_base *next{old.next};
	for(; next->next; next = next->next);

	assert(!next->next);
	next->next = &new_;
}

/// Internal use; chases the linked list of promises starting from the head in
/// the shared_state and updates the location of the shared_state within each
/// promise. This is used to tell the promises when the shared_state itself
/// has relocated.
void
ircd::ctx::set_promises_state(shared_state_base &st)
{
	assert(is(st, future_state::PENDING));
	promise_base *next
	{
		promise_base::head(st)
	};

	assert(next);
	for(; next; next = next->next)
		next->st = std::addressof(st);
}

/// Chases the linked list of promises starting from the head
/// in the shared_state and invalidates all of their references to the shared
/// state. This will cause the promise to no longer be valid().
///
void
ircd::ctx::invalidate_promises(shared_state_base &st)
{
	promise_base *next
	{
		promise_base::head(st)
	};

	for(; next; next = next->next)
		next->st = nullptr;
}

///////////////////////////////////////////////////////////////////////////////
//
// condition_variable
//

/// Wake up the next context waiting on the condition_variable
///
/// Unlike notify_one(), the next context in the queue is repositioned in the
/// back before being woken up for fairness.
void
ircd::ctx::condition_variable::notify()
noexcept
{
	ctx *c;
	if(!(c = q.pop_front()))
		return;

	q.push_back(c);
	ircd::ctx::notify(*c);
}

/// Wake up the next context waiting on the dock
void
ircd::ctx::condition_variable::notify_one()
noexcept
{
	if(q.empty())
		return;

	ircd::ctx::notify(*q.front());
}

/// Wake up all contexts waiting on the condition_variable.
///
/// We post all notifications without requesting direct context
/// switches. This ensures everyone gets notified in a single
/// transaction without any interleaving during this process.
void
ircd::ctx::condition_variable::notify_all()
noexcept
{
	q.for_each([this](ctx &c)
	noexcept
	{
		ircd::ctx::notify(c);
	});
}

/// Wake up all contexts waiting on the condition_variable to throw an
/// interrupt exception.
void
ircd::ctx::condition_variable::interrupt_all()
noexcept
{
	q.for_each([this](ctx &c)
	noexcept
	{
		ircd::ctx::interrupt(c);
	});
}

/// Wake up all contexts waiting on the condition_variable to throw an
/// interrupt exception.
void
ircd::ctx::condition_variable::terminate_all()
noexcept
{
	q.for_each([this](ctx &c)
	noexcept
	{
		ircd::ctx::terminate(c);
	});
}

/// The number of contexts waiting in the queue.
bool
ircd::ctx::condition_variable::waiting(const ctx &a)
const noexcept
{
	// for_each returns false if a was found
	return !q.for_each(list::closure_bool_const{[&a](const ctx &b)
	noexcept
	{
		// return false to break on equal
		return std::addressof(a) != std::addressof(b);
	}});
}

///////////////////////////////////////////////////////////////////////////////
//
// dock.h
//

/// Wake up the next context waiting on the dock
///
/// Unlike notify_one(), the next context in the queue is repositioned in the
/// back before being woken up for fairness.
void
ircd::ctx::dock::notify()
noexcept
{
	ctx *c;
	if(!(c = q.pop_front()))
		return;

	q.push_back(c);
	ircd::ctx::notify(*c);
}

/// Wake up all contexts waiting on the dock.
///
/// We post all notifications without requesting direct context
/// switches. This ensures everyone gets notified in a single
/// transaction without any interleaving during this process.
void
ircd::ctx::dock::notify_all()
noexcept
{
	q.for_each([this](ctx &c)
	noexcept
	{
		ircd::ctx::notify(c);
	});
}

/// Wake up all contexts waiting on the dock to throw an interrupt exception.
void
ircd::ctx::dock::interrupt_all()
noexcept
{
	q.for_each([this](ctx &c)
	noexcept
	{
		ircd::ctx::interrupt(c);
	});
}

/// Wake up all contexts waiting on the dock to throw an interrupt exception.
void
ircd::ctx::dock::terminate_all()
noexcept
{
	q.for_each([this](ctx &c)
	noexcept
	{
		ircd::ctx::terminate(c);
	});
}

[[gnu::hot]]
void
ircd::ctx::dock::wait()
{
	assert(current);
	const unwind_exceptional renotify{[this]
	{
		notify_one();
	}};

	const unwind remove{[this]
	{
		q.remove(current);
	}};

	q.push_back(current);
	this_ctx::wait();
}

void
ircd::ctx::dock::wait(const predicate &pred)
{
	if(pred())
		return;

	assert(current);
	const unwind_exceptional renotify{[this]
	{
		notify_one();
	}};

	const unwind remove{[this]
	{
		q.remove(current);
	}};

	q.push_back(current); do
	{
		this_ctx::wait();
	}
	while(!pred());
}

/// The number of contexts waiting in the queue.
bool
ircd::ctx::dock::waiting(const ctx &a)
const noexcept
{
	// for_each returns false if a was found
	return !q.for_each(list::closure_bool_const{[&a](const ctx &b)
	noexcept
	{
		// return false to break on equal
		return std::addressof(a) != std::addressof(b);
	}});
}

/// The number of contexts waiting in the queue.
size_t
ircd::ctx::dock::size()
const noexcept
{
	return q.size();
}

/// The number of contexts waiting in the queue.
bool
ircd::ctx::dock::empty()
const noexcept
{
	return q.empty();
}

///////////////////////////////////////////////////////////////////////////////
//
// ctx_list.h
//

void
ircd::ctx::list::remove(ctx *const &c)
noexcept
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
noexcept
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
noexcept
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
noexcept
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
noexcept
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

size_t
ircd::ctx::list::size()
const noexcept
{
	size_t i{0};
	for_each([&i](const ctx &)
	noexcept
	{
		++i;
	});

	return i;
}

void
ircd::ctx::list::rfor_each(const closure &closure)
{
	for(ctx *tail{this->tail}; tail; tail = prev(tail))
		closure(*tail);
}

void
ircd::ctx::list::rfor_each(const closure_const &closure)
const
{
	for(const ctx *tail{this->tail}; tail; tail = prev(tail))
		closure(*tail);
}

bool
ircd::ctx::list::rfor_each(const closure_bool &closure)
{
	for(ctx *tail{this->tail}; tail; tail = prev(tail))
		if(!closure(*tail))
			return false;

	return true;
}

bool
ircd::ctx::list::rfor_each(const closure_bool_const &closure)
const
{
	for(const ctx *tail{this->tail}; tail; tail = prev(tail))
		if(!closure(*tail))
			return false;

	return true;
}

void
ircd::ctx::list::for_each(const closure &closure)
{
	for(ctx *head{this->head}; head; head = next(head))
		closure(*head);
}

void
ircd::ctx::list::for_each(const closure_const &closure)
const
{
	for(const ctx *head{this->head}; head; head = next(head))
		closure(*head);
}

bool
ircd::ctx::list::for_each(const closure_bool &closure)
{
	for(ctx *head{this->head}; head; head = next(head))
		if(!closure(*head))
			return false;

	return true;
}

bool
ircd::ctx::list::for_each(const closure_bool_const &closure)
const
{
	for(const ctx *head{this->head}; head; head = next(head))
		if(!closure(*head))
			return false;

	return true;
}

[[gnu::hot]]
ircd::ctx::list::node &
ircd::ctx::list::get(ctx &c)
noexcept
{
	return c.node;
}

[[gnu::hot]]
const ircd::ctx::list::node &
ircd::ctx::list::get(const ctx &c)
noexcept
{
	return c.node;
}

//////////////////////////////////////////////////////////////////////////////
//
// ctx/stack.h
//

[[gnu::hot]]
ircd::ctx::stack &
ircd::ctx::stack::get(ctx &ctx)
noexcept
{
	return ctx.stack;
}

[[gnu::hot]]
const ircd::ctx::stack &
ircd::ctx::stack::get(const ctx &ctx)
noexcept
{
	return ctx.stack;
}

//
// stack::stack
//

ircd::ctx::stack::stack(const mutable_buffer &buf)
noexcept
:buf
{
	buf
}
,max
{
	//note: ircd::size() asserts because begin(buf) is nullptr, but that's ok
	//ircd::size(buf)
	size_t(std::distance(begin(buf), end(buf)))
}
{
}

//
// stack::allocator
//

struct [[gnu::visibility("hidden")]]
ircd::ctx::stack::allocator
{
	using stack_context = boost::coroutines::stack_context;

	mutable_buffer &buf;
	bool owner {false};

	void allocate(stack_context &, size_t size);
	void deallocate(stack_context &);
};

void
ircd::ctx::stack::allocator::allocate(stack_context &c,
                                      size_t size)
{
	static const auto &alignment
	{
		info::page_size
	};

	unique_mutable_buffer umb
	{
		null(this->buf)? size: 0, alignment
	};

	const mutable_buffer &buf
	{
		umb? umb: this->buf
	};

	c.size = ircd::size(buf);
	c.sp = ircd::data(buf) + c.size;

	#if defined(BOOST_USE_VALGRIND)
	if(vg::active)
		c.valgrind_stack_id = vg::stack::add(buf);
	#endif

	this->owner = bool(umb);
	this->buf = umb? umb.release(): this->buf;
}

void
ircd::ctx::stack::allocator::deallocate(stack_context &c)
{
	assert(c.sp);

	#if defined(BOOST_USE_VALGRIND)
	if(vg::active)
		vg::stack::del(c.valgrind_stack_id);
	#endif

	const auto base
	{
		(reinterpret_cast<uintptr_t>(c.sp) - c.size)
		& boolmask<uintptr_t>(owner)
	};

	std::free(reinterpret_cast<void *>(base));
}

///////////////////////////////////////////////////////////////////////////////
//
// (internal) boost::asio
//

template<class Function>
struct [[gnu::visibility("hidden")]]
boost::asio::detail::spawn_data
<
	boost::asio::executor_binder<void (*)(), boost::asio::strand<boost::asio::executor>>,
	Function
>
{
	using Handler = boost::asio::executor_binder<void (*)(), boost::asio::strand<boost::asio::executor>>;
	using caller_type = typename basic_yield_context<Handler>::caller_type;
	using callee_type = typename basic_yield_context<Handler>::callee_type;

	weak_ptr<callee_type> coro_;
	Function function_;
	Handler handler_;
	ircd::ctx::ctx *ctrl;

	template<class H,
	         class F>
	spawn_data(H&& handler, bool call_handler, F&& function)
	:function_(std::move(function))
	,handler_(std::move(handler))
	,ctrl{ircd::ctx::ctx::spawning}
	{
		assert(call_handler);
		assert(ctrl);
	}
};

template<class Function>
struct [[gnu::visibility("hidden")]]
boost::asio::detail::coro_entry_point
<
	boost::asio::executor_binder<void (*)(), boost::asio::strand<boost::asio::executor>>,
	Function
>
{
	using Handler = boost::asio::executor_binder<void (*)(), boost::asio::strand<boost::asio::executor>>;
	using caller_type = typename basic_yield_context<Handler>::caller_type;

	void operator()(caller_type &ca) // pull
	{
		const auto data
		{
			this->data
		};

		basic_yield_context<Handler> yc
		{
			data->coro_, ca, data->handler_
		};

		(data->function_)(yc);
		(data->handler_)();
	}

	shared_ptr<spawn_data<Handler, Function>> data;
};

// Hooks the first push phase of coroutine spawn to supply our own stack
// allocator.
template<class Function>
struct [[gnu::visibility("hidden")]]
boost::asio::detail::spawn_helper
<
	boost::asio::executor_binder<void (*)(), boost::asio::strand<boost::asio::executor>>,
	Function
>
{
	using Handler = boost::asio::executor_binder<void (*)(), boost::asio::strand<boost::asio::executor>>;
	using callee_type = typename basic_yield_context<Handler>::callee_type;

	void operator()() // push
	{
		const auto coro
		{
			std::make_shared<callee_type>
			(
				coro_entry_point<Handler, Function>{data_},
				attributes_,
				ircd::ctx::stack::allocator{data_->ctrl->stack.buf}
			)
		};

		data_->coro_ = coro;
		(*coro)();
	}

	shared_ptr<spawn_data<Handler, Function>> data_;
	boost::coroutines::attributes attributes_;
};

///////////////////////////////////////////////////////////////////////////////
//
// (internal) boost::asio
//

//
// Optimize ctx::wake() by reimplementing the timer cancel's op scheduler to
// enqueue as a defer (private/priority queue) rather than to the post queue.
// This interposes the function for all callstacks in this translation unit,
// including primarily ctx::wake().
//
#if defined(BOOST_ASIO_HAS_EPOLL)
using epoll_time_traits = boost::asio::time_traits<boost::posix_time::ptime>;

template<>
std::size_t
__attribute__((visibility("internal")))
boost::asio::detail::epoll_reactor::cancel_timer(timer_queue<epoll_time_traits> &queue,
                                                 typename timer_queue<epoll_time_traits>::per_timer_data &t,
                                                 std::size_t max)
{
	constexpr bool post_priv {true};
	constexpr bool post_defer {false};
	constexpr bool do_dispatch {false};
	constexpr bool poll_dispatch {false};
	constexpr bool is_continuation {false};

	auto *const thread_info
	{
		static_cast<scheduler_thread_info *>(scheduler::thread_call_stack::top())
	};

	std::size_t ret;
	op_queue<operation> ops;
	{
		const mutex::scoped_lock lock(mutex_);
		ret = queue.cancel_timer(t, ops, max);
	}

	if constexpr(post_priv)
		thread_info->private_op_queue.push(ops);

	else if constexpr(post_defer)
		scheduler_.post_deferred_completions(ops);

	else for(auto *op(ops.front()); op; ops.pop(), op = ops.front())
	{
		if constexpr(do_dispatch)
		{
			scheduler_.do_dispatch(op);
			continue;
		}

		scheduler_.post_immediate_completion(op, is_continuation);
		thread_info->private_outstanding_work -= is_continuation;
	}

	return ret;
}
#endif
