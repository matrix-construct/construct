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

/// Internal context implementation
///
struct ircd::ctx::ctx
:instance_list<ctx>
{
	using error_code = boost::system::error_code;

	static uint64_t id_ctr;                      // monotonic

	uint64_t id;                                 // Unique runtime ID
	const char *name;                            // User given name (optional)
	context::flags flags;                        // User given flags
	boost::asio::io_service::strand strand;      // mutex/serializer
	boost::asio::steady_timer alarm;             // acting semaphore (64B)
	boost::asio::yield_context *yc;              // boost interface
	uintptr_t stack_base;                        // assigned when spawned
	size_t stack_max;                            // User given stack size
	int64_t notes;                               // norm: 0 = asleep; 1 = awake; inc by others; dec by self
	continuation *cont;                          // valid when asleep; invalid when awake
	ctx *adjoindre;                              // context waiting for this to join()
	microseconds awake;                          // monotonic counter
	list::node node;                             // node for ctx::list

	bool started() const                         { return stack_base != 0;                         }
	bool finished() const                        { return started() && yc == nullptr;              }

	bool interruption_point(std::nothrow_t);     // Check for interrupt (and clear flag)
	void interruption_point();                   // throws interrupted

	bool wait();                                 // yield context to ios queue (returns on this resume)
	void jump();                                 // jump to context directly (returns on your resume)
	void wake();                                 // jump to context by queueing with ios (use note())
	bool note();                                 // properly request wake()

	void operator()(boost::asio::yield_context, const std::function<void ()>) noexcept;

	ctx(const char *const &name                  = "<noname>",
	    const size_t &stack_max                  = DEFAULT_STACK_SIZE,
	    const context::flags &flags              = (context::flags)0,
	    boost::asio::io_service *const &ios      = ircd::ios);

	ctx(ctx &&) = delete;
	ctx(const ctx &) = delete;
	ctx &operator=(ctx &&) = delete;
	ctx &operator=(const ctx &) = delete;
};

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

ircd::ctx::ctx::ctx(const char *const &name,
                    const size_t &stack_max,
                    const context::flags &flags,
                    boost::asio::io_service *const &ios)
:id{++id_ctr}
,name{name}
,flags{flags}
,strand{*ios}
,alarm{*ios}
,yc{nullptr}
,stack_base{0}
,stack_max{stack_max}
,notes{1}
,cont{nullptr}
,adjoindre{nullptr}
,awake{0us}
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
	stack_base = uintptr_t(__builtin_frame_address(0));
	ircd::ctx::current = this;
	mark(prof::event::CUR_ENTER);

	const unwind atexit([this]
	{
		mark(prof::event::CUR_LEAVE);

		if(adjoindre)
			notify(*adjoindre);

		ircd::ctx::current = nullptr;
		this->yc = nullptr;

		if(flags & context::DETACH)
			delete this;
	});

	// Check for a precocious interrupt
	if(unlikely(flags & context::INTERRUPTED))
		return;

	if(likely(bool(func)))
		func();
}
catch(const std::exception &e)
{
	log::critical
	{
		"ctx(%p '%s' #%u): unhandled: %s",
		current,
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
		const continuation continuation{current};
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

	const auto interruption{[this]
	(ctx *const &interruptor) noexcept
	{
		wake();
	}};

	boost::system::error_code ec;
	alarm.async_wait(boost::asio::yield_context{to_asio{interruption}}[ec]);

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

	wake();
	return true;
}

/// Wakes a context without a note (internal)
void
ircd::ctx::ctx::wake()
try
{
	alarm.cancel();
}
catch(const boost::system::system_error &e)
{
	log::error
	{
		"ctx::wake(%p): %s", this, e.what()
	};
}

/// Throws if this context has been flagged for interruption and clears
/// the flag.
void
ircd::ctx::ctx::interruption_point()
{
	if(unlikely(interruption_point(std::nothrow)))
		throw interrupted("ctx(%p) '%s'", (const void *)this, name);
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
		mark(prof::event::CUR_INTERRUPT);
		flags &= ~context::INTERRUPTED;
		return true;
	}
	else return false;
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

/// Marks `ctx` for interruption and enqueues it for resumption to receive the
/// interrupt which will be an exception coming out of the point where the
/// `ctx` was yielding.
void
ircd::ctx::interrupt(ctx &ctx)
{
	if(finished(ctx))
		return;

	if(interruption(ctx))
		return;

	ctx.flags |= context::INTERRUPTED;
	if(likely(&ctx != current && ctx.cont != nullptr))
		ctx.cont->interrupted(current);
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

/// Indicates if `ctx` was interrupted; does not clear the flag
bool
ircd::ctx::interruption(const ctx &c)
{
	return c.flags & context::INTERRUPTED;
}

/// Returns the notification count for `ctx
const int64_t &
ircd::ctx::notes(const ctx &ctx)
{
	return ctx.notes;
}

/// Returns the notification count for `ctx
const size_t &
ircd::ctx::stack_max(const ctx &ctx)
{
	return ctx.stack_max;
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
ircd::ctx::this_ctx::sleep_until(const std::chrono::steady_clock::time_point &tp)
{
	while(!wait_until(tp, std::nothrow));
}

/// Yield the currently running context until notified or `time_point`.
///
/// Returns true if this function returned because `time_point` was hit or
/// false because this context was notified.
bool
ircd::ctx::this_ctx::wait_until(const std::chrono::steady_clock::time_point &tp,
                                const std::nothrow_t &)
{
	auto &c(cur());
	c.alarm.expires_at(tp);
	c.wait(); // now you're yielding with portals

	return std::chrono::steady_clock::now() >= tp;
}

/// Yield the currently running context for `duration` or until notified.
///
/// Returns the duration remaining if notified, or <= 0 if suspended for
/// the full duration, or unchanged if no suspend ever took place.
std::chrono::microseconds
ircd::ctx::this_ctx::wait(const std::chrono::microseconds &duration,
                          const std::nothrow_t &)
{
	auto &c(cur());
	c.alarm.expires_from_now(duration);
	c.wait(); // now you're yielding with portals
	const auto ret(c.alarm.expires_from_now());

	// return remaining duration.
	// this is > 0 if notified
	// this is unchanged if a note prevented any wait at all
	return std::chrono::duration_cast<std::chrono::microseconds>(ret);
}

/// Yield the currently running context until notified.
void
ircd::ctx::this_ctx::wait()
{
	auto &c(cur());
	c.alarm.expires_at(std::chrono::steady_clock::time_point::max());
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
	ios->post(restore); do
	{
		wait();
	}
	while(!done);
}

/// Throws interrupted if the currently running context was interrupted
/// and clears the interrupt flag.
void
ircd::ctx::this_ctx::interruption_point()
{
	return cur().interruption_point();
}

/// Returns true if the currently running context was interrupted and clears
/// the interrupt flag.
bool
ircd::ctx::this_ctx::interruption_requested()
{
	return interruption(cur());
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
}

ircd::ctx::this_ctx::critical_assertion::critical_assertion()
:theirs{critical_asserted}
{
	critical_asserted = true;
}

ircd::ctx::this_ctx::critical_assertion::~critical_assertion()
noexcept
{
	assert(critical_asserted);
	critical_asserted = theirs;
}

///////////////////////////////////////////////////////////////////////////////
//
// ctx/continuation.h
//

//
// continuation
//

ircd::ctx::continuation::continuation(ctx *const &self)
:self{self}
{
	mark(prof::event::CUR_YIELD);
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

void
ircd::ctx::continuation::interrupted(ctx *const &interruptor)
noexcept
{
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

void
ircd::ctx::to_asio::interrupted(ctx *const &interruptor)
noexcept
{
	if(handler)
		handler(interruptor);
}

///////////////////////////////////////////////////////////////////////////////
//
// ctx/context.h
//

namespace ircd::ctx
{
	static void spawn(ctx *const c, context::function func);
}

void
ircd::ctx::spawn(ctx *const c,
                 context::function func)
{
	const boost::coroutines::attributes attrs
	{
		c->stack_max,
		boost::coroutines::stack_unwind
	};

	auto bound
	{
		std::bind(&ctx::operator(), c, ph::_1, std::move(func))
	};

	boost::asio::spawn(c->strand, std::move(bound), attrs);
}

ircd::ctx::context::context(const char *const &name,
                            const size_t &stack_sz,
                            const flags &flags,
                            function func)
:c{std::make_unique<ctx>(name, stack_sz, flags, ircd::ios)}
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
		ios->post(std::move(spawn));
		return;
	}

	// The current context must be reasserted if spawn returns here
	auto *const theirs(ircd::ctx::current);
	const unwind recurrent([&theirs]
	{
		ircd::ctx::current = theirs;
	});

	if(flags & DISPATCH)
		ios->dispatch(std::move(spawn));
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

ircd::ctx::context::~context()
noexcept
{
	if(!c)
		return;

	// Can't join to bare metal, only from within another context.
	if(current)
	{
		interrupt();
		join();
	}

	// because *this uses unique_ptr's, if we dtor the ircd::ctx from
	// right here and ircd::ctx hasn't been entered yet because the user
	// passed the POST flag, the ctx::spawn() is still sitting in the ios
	// queue.
	if(c && !started(*c))
	{
		c->flags |= context::DETACH;
		c.release();
	}
}

void
ircd::ctx::context::join()
{
	if(joined())
		return;

	mark(prof::event::JOIN);
	assert(bool(c));
	assert(!c->adjoindre);
	c->adjoindre = &cur();       // Set the target context to notify this context when it finishes
	wait();
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
	del(size());
}

void
ircd::ctx::pool::operator()(closure closure)
{
	queue.push_back(std::move(closure));
	dock.notify();
}

void
ircd::ctx::pool::del(const size_t &num)
{
	const ssize_t requested(size() - num);
	const size_t target(std::max(requested, ssize_t(0)));
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
	del(size());
}

void
ircd::ctx::pool::interrupt()
{
	for(auto &context : ctxs)
		context.interrupt();
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
/*
	log::debug
	{
		"pool(%p) ctx(%p): %s", this, &cur(), e.what()
	};
*/
}

void
ircd::ctx::pool::next()
try
{
	dock.wait([this]
	{
		return !queue.empty();
	});

	++working;
	const unwind avail([this]
	{
		--working;
	});

	const auto func(std::move(queue.front()));
	queue.pop_front();
	func();
}
catch(const interrupted &e)
{
	throw;
}
catch(const std::exception &e)
{
	log::critical
	{
		"pool(%p) ctx(%p '%s' #%u): unhandled: %s",
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
	time_point cur_slice_start;     // Time slice state
	uint64_t cur_slice_rdtsc;       // Time slice state

	void check_stack();
	void check_slice();
	void slice_start();

	void handle_cur_continue();
	void handle_cur_yield();
	void handle_cur_leave();
	void handle_cur_enter();
}

struct ircd::ctx::prof::settings ircd::ctx::prof::settings
{
	0.33,        // stack_usage_warning at 1/3 engineering tolerance
	0.50,        // stack_usage_assertion at 1/2 engineering tolerance

	50ms,        // slice_warning at 1/20 slices per second
	0us,         // slice_interrupt unused until project more mature...
	0us,         // slice_assertion unused; warning sufficient for now...
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
	check_stack();
	check_slice();
}

void
ircd::ctx::prof::handle_cur_continue()
{
	slice_start();
}

void
ircd::ctx::prof::slice_start()
{
	cur_slice_rdtsc = __rdtsc();
	cur_slice_start = steady_clock::now();
}

void
ircd::ctx::prof::check_slice()
{
	const uint64_t now_rdtsc(__rdtsc());
	const uint64_t rdtsc_usage(now_rdtsc - cur_slice_rdtsc);

	const auto now_sc(steady_clock::now());
	const auto time_usage(now_sc - cur_slice_start);

	auto &c(cur());
	c.awake += duration_cast<microseconds>(time_usage);

	if(unlikely(settings.slice_warning > 0us && time_usage >= settings.slice_warning))
	{
		log::dwarning
		{
			"context timeslice exceeded '%s' #%lu total: %06ld$us last: %lu$ns %lu$tsc",
			name(c),
			id(c),
			c.awake.count(),
			duration_cast<nanoseconds>(time_usage).count(),
			rdtsc_usage
		};

		assert(settings.slice_assertion == 0us || time_usage < settings.slice_assertion);
	}

	if(unlikely(settings.slice_interrupt > 0us && time_usage >= settings.slice_interrupt))
		throw interrupted
		{
			"Time slice exceeded '%s' #%lu (last: %06ld microseconds)",
			name(c),
			id(c),
			duration_cast<microseconds>(time_usage).count()
		};
}

void
ircd::ctx::prof::check_stack()
{
	auto &c(cur());
	const double &stack_max(c.stack_max);
	const auto stack_usage(stack_usage_here(c));

	if(unlikely(stack_usage > stack_max * settings.stack_usage_warning))
	{
		log::dwarning
		{
			"context stack usage ctx '%s' #%lu used %zu of %zu bytes",
			name(c),
			id(c),
			stack_usage,
			c.stack_max
		};

		assert(stack_usage < c.stack_max * settings.stack_usage_assertion);
	}
}

size_t
ircd::ctx::stack_usage_here()
{
	assert(current);
	return stack_usage_here(*current);
}

size_t
ircd::ctx::stack_usage_here(const ctx &ctx)
{
	return ctx.stack_base - uintptr_t(__builtin_frame_address(0));
}

///////////////////////////////////////////////////////////////////////////////
//
// ctx_ole.h
//

namespace ircd::ctx::ole
{
	using closure = std::function<void () noexcept>;

	std::mutex mutex;
	std::condition_variable cond;
	std::deque<closure> queue;
	bool interruption;
	std::thread *thread;

	closure pop();
	void worker() noexcept;
	void push(closure &&);
}

ircd::ctx::ole::init::init()
{
	assert(!thread);
	interruption = false;
}

ircd::ctx::ole::init::~init()
noexcept
{
	if(!thread)
		return;

	mutex.lock();
	interruption = true;
	cond.notify_one();
	mutex.unlock();
	thread->join();
	delete thread;
	thread = nullptr;
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

	push(std::move(closure)); do
	{
		wait();
	}
	while(!done);

	if(eptr)
		std::rethrow_exception(eptr);
}

void
ircd::ctx::ole::push(closure &&func)
{
	if(unlikely(!thread))
		thread = new std::thread(&worker);

	const std::lock_guard<decltype(mutex)> lock(mutex);
	queue.emplace_back(std::move(func));
	cond.notify_one();
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
	return;
}

ircd::ctx::ole::closure
ircd::ctx::ole::pop()
{
	std::unique_lock<decltype(mutex)> lock(mutex);
	cond.wait(lock, []
	{
		if(!queue.empty())
			return true;

		if(unlikely(interruption))
			throw interrupted();

		return false;
	});

	auto c(std::move(queue.front()));
	queue.pop_front();
	return std::move(c);
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
		return tail;

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
		return head;

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

const ircd::ctx::ctx *const &
ircd::ctx::list::prev(const ctx *const &c)
{
	assert(c);
	const auto &node(c->node);
	const ctx *const &ptr(node.prev);
	return ptr;
}

const ircd::ctx::ctx *const &
ircd::ctx::list::next(const ctx *const &c)
{
	assert(c);
	const auto &node(c->node);
	const ctx *const &ptr(node.next);
	return ptr;
}

///////////////////////////////////////////////////////////////////////////////
//
// ircd/ios.h
//

void
ircd::post(std::function<void ()> function)
{
	ircd::ios->post(std::move(function));
}

void
ircd::dispatch(std::function<void ()> function)
{
	ircd::ios->dispatch(std::move(function));
}
