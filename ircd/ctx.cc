/*
 *  Copyright (C) 2016 Charybdis Development Team
 *  Copyright (C) 2016 Jason Volk <jason@zemos.net>
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

#include <boost/asio/steady_timer.hpp>
#include <ircd/ctx/continuation.h>

///////////////////////////////////////////////////////////////////////////////
//
// (internal)
//

namespace ircd {
namespace ctx  {

struct ctx
{
	using error_code = boost::system::error_code;

	static uint64_t id_ctr;                      // monotonic

	uint64_t id;                                 // Unique runtime ID
	const char *name;                            // User given name (optional)
	context::flags flags;                        // User given flags
	boost::asio::steady_timer alarm;             // acting semaphore (64B)
	boost::asio::yield_context *yc;              // boost interface
	uintptr_t stack_base;                        // assigned when spawned
	size_t stack_max;                            // User given stack size
	int64_t notes;                               // norm: 0 = asleep; 1 = awake; inc by others; dec by self
	ctx *adjoindre;                              // context waiting for this to join()
	microseconds awake;                          // monotonic counter

	bool finished() const                        { return yc == nullptr;                           }
	bool started() const                         { return bool(yc);                                }
	bool wait();                                 // suspend this context (returns on resume)
	void wake();                                 // queue resumption of this context (use note())
	bool note();                                 // properly request wake()

	void operator()(boost::asio::yield_context, const std::function<void ()>) noexcept;

	ctx(const char *const &name                  = "<unnamed context>",
	    const size_t &stack_max                  = DEFAULT_STACK_SIZE,
	    const context::flags &flags              = (context::flags)0,
	    boost::asio::io_service *const &ios      = ircd::ios);

	ctx(ctx &&) noexcept = delete;
	ctx(const ctx &) = delete;
};

} // namespace ctx
} // namespace ircd

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
,alarm{*ios}
,yc{nullptr}
,stack_base{0}
,stack_max{stack_max}
,notes{1}
,adjoindre{nullptr}
,awake{0us}
{
}

void
ircd::ctx::ctx::operator()(boost::asio::yield_context yc,
                           const std::function<void ()> func)
noexcept
{
	this->yc = &yc;
	notes = 1;
	stack_base = uintptr_t(__builtin_frame_address(0));
	ircd::ctx::current = this;
	mark(prof::event::CUR_ENTER);

	const scope atexit([this]
	{
		mark(prof::event::CUR_LEAVE);

		if(adjoindre)
			notify(*adjoindre);

		ircd::ctx::current = nullptr;
		this->yc = nullptr;

		if(flags & context::DETACH)
			delete this;
	});

	if(likely(bool(func)))
		func();
}

bool
ircd::ctx::ctx::note()
{
	if(notes++ > 0)
		return false;

	wake();
	return true;
}

void
ircd::ctx::ctx::wake()
try
{
	alarm.cancel();
}
catch(const boost::system::system_error &e)
{
	ircd::log::error("ctx::wake(%p): %s", this, e.what());
}

bool
ircd::ctx::ctx::wait()
{
	if(--notes > 0)
		return false;

	boost::system::error_code ec;
	alarm.async_wait(boost::asio::yield_context(continuation(this))[ec]);

	assert(ec == boost::system::errc::operation_canceled ||
	       ec == boost::system::errc::success);

	// Interruption shouldn't be used for normal operation,
	// so please eat this branch misprediction.
	if(unlikely(flags & context::INTERRUPTED))
	{
		mark(prof::event::CUR_INTERRUPT);
		flags &= ~context::INTERRUPTED;
		throw interrupted("ctx(%p)::wait()", (const void *)this);
	}

	// notes = 1; set by continuation dtor on wakeup
	return true;
}

///////////////////////////////////////////////////////////////////////////////
//
// ctx/ctx.h
//
__thread ircd::ctx::ctx *ircd::ctx::current;

void
ircd::ctx::sleep_until(const std::chrono::steady_clock::time_point &tp)
{
	while(!wait_until(tp, std::nothrow));
}

bool
ircd::ctx::wait_until(const std::chrono::steady_clock::time_point &tp,
                const std::nothrow_t &)
{
	auto &c(cur());
	c.alarm.expires_at(tp);
	c.wait(); // now you're yielding with portals

	return std::chrono::steady_clock::now() >= tp;
}

std::chrono::microseconds
ircd::ctx::wait(const std::chrono::microseconds &duration,
          const std::nothrow_t &)
{
	auto &c(cur());
	c.alarm.expires_from_now(duration);
	c.wait(); // now you're yielding with portals
	const auto ret(c.alarm.expires_from_now());

	// return remaining duration.
	// this is > 0 if notified or interrupted
	// this is unchanged if a note prevented any wait at all
	return std::chrono::duration_cast<std::chrono::microseconds>(ret);
}

void
ircd::ctx::wait()
{
	auto &c(cur());
	c.alarm.expires_at(std::chrono::steady_clock::time_point::max());
	c.wait(); // now you're yielding with portals
}

void
ircd::ctx::yield()
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

bool
ircd::ctx::notify(ctx &ctx)
{
	return ctx.note();
}

void
ircd::ctx::interrupt(ctx &ctx)
{
	ctx.flags |= context::INTERRUPTED;
	ctx.wake();
}

bool
ircd::ctx::started(const ctx &ctx)
{
	return ctx.started();
}

bool
ircd::ctx::finished(const ctx &ctx)
{
	return ctx.finished();
}

const int64_t &
ircd::ctx::notes(const ctx &ctx)
{
	return ctx.notes;
}

ircd::string_view
ircd::ctx::name(const ctx &ctx)
{
	return ctx.name;
}

const uint64_t &
ircd::ctx::id(const ctx &ctx)
{
	return ctx.id;
}

///////////////////////////////////////////////////////////////////////////////
//
// ctx/continuation.h
//

ircd::ctx::continuation::continuation(ctx *const &self)
:self{self}
{
    mark(prof::event::CUR_YIELD);
    assert(self != nullptr);
    assert(self->notes <= 1);
    ircd::ctx::current = nullptr;
}

ircd::ctx::continuation::~continuation()
noexcept
{
    ircd::ctx::current = self;
    self->notes = 1;
    mark(prof::event::CUR_CONTINUE);
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

///////////////////////////////////////////////////////////////////////////////
//
// ctx/context.h
//

ircd::ctx::context::context(const char *const &name,
                            const size_t &stack_sz,
                            const flags &flags,
                            std::function<void ()> func)
:c{std::make_unique<ctx>(name, stack_sz, flags, ircd::ios)}
{
	auto spawn([stack_sz, c(c.get()), func(std::move(func))]
	{
		auto bound(std::bind(&ctx::operator(), c, ph::_1, std::move(func)));
		const boost::coroutines::attributes attrs
		{
			stack_sz,
			boost::coroutines::stack_unwind
		};

		boost::asio::spawn(*ios, std::move(bound), attrs);
	});

	// The current context must be reasserted if spawn returns here
	const scope recurrent([current(ircd::ctx::current)]
	{
		ircd::ctx::current = current;
	});

	// The profiler is told about the spawn request here, not inside the closure
	// which is probably the same event-slice as event::CUR_ENTER and not as useful.
	mark(prof::event::SPAWN);

	if(flags & POST)
		ios->post(std::move(spawn));
	else if(flags & DISPATCH)
		ios->dispatch(std::move(spawn));
	else
		spawn();

	if(flags & DETACH)
		c.release();
}

ircd::ctx::context::context(const char *const &name,
                            const size_t &stack_size,
                            std::function<void ()> func,
                            const flags &flags)
:context
{
	name, stack_size, flags, std::move(func)
}
{
}

ircd::ctx::context::context(const char *const &name,
                            const flags &flags,
                            std::function<void ()> func)
:context
{
	name, DEFAULT_STACK_SIZE, flags, std::move(func)
}
{
}

ircd::ctx::context::context(const char *const &name,
                            std::function<void ()> func,
                            const flags &flags)
:context
{
	name, DEFAULT_STACK_SIZE, flags, std::move(func)
}
{
}

ircd::ctx::context::context(std::function<void ()> func,
                            const flags &flags)
:context
{
	"<unnamed context>", DEFAULT_STACK_SIZE, flags, std::move(func)
}
{
}

ircd::ctx::context::~context()
noexcept
{
	if(!c)
		return;

	// Can't join to bare metal, only from within another context.
	if(!current)
		return;

	interrupt();
	join();
}

void
ircd::ctx::context::join()
{
	if(joined())
		return;

	mark(prof::event::JOIN);
	assert(!c->adjoindre);
	c->adjoindre = &cur();       // Set the target context to notify this context when it finishes
	wait();
	mark(prof::event::JOINED);
}

ircd::ctx::ctx *
ircd::ctx::context::detach()
{
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
,available{0}
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
	dock.notify_one();
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
ircd::ctx::pool::interrupt()
{
	for(auto &context : ctxs)
		context.interrupt();
}

void
ircd::ctx::pool::main()
try
{
	++available;
	const scope avail([this]
	{
		--available;
	});

	while(1)
		next();
}
catch(const interrupted &e)
{
	log::debug("pool(%p) ctx(%p): %s",
	           this,
	           &cur(),
	           e.what());
}

void
ircd::ctx::pool::next()
try
{
	dock.wait([this]
	{
		return !queue.empty();
	});

	--available;
	const scope avail([this]
	{
		++available;
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
	log::critical("pool(%p) ctx(%p): unhandled: %s",
	              this,
	              &cur(),
	              e.what());
}

///////////////////////////////////////////////////////////////////////////////
//
// ctx_prof.h
//

namespace ircd {
namespace ctx  {
namespace prof {

struct settings settings
{
	0.46,        // stack_usage_warning
	0.67,        // stack_usage_assertion

	50ms,        // slice_warning
	0us,         // slice_interrupt
	0us,         // slice_assertion
};

time_point cur_slice_start;     // Time slice state

size_t stack_usage_here(const ctx &) __attribute__((noinline));
void check_stack();
void check_slice();
void slice_start();

void handle_cur_continue();
void handle_cur_yield();
void handle_cur_leave();
void handle_cur_enter();

} // namespace prof
} // namespace ctx
} // namespace ircd

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
	cur_slice_start = steady_clock::now();
}

void
ircd::ctx::prof::check_slice()
{
	auto &c(cur());

	const auto time_usage(steady_clock::now() - cur_slice_start);
	c.awake += duration_cast<microseconds>(time_usage);

	if(unlikely(settings.slice_warning > 0us && time_usage >= settings.slice_warning))
	{
		log::warning("CONTEXT TIMESLICE EXCEEDED (%p) '%s' last: %06ld$us total: %06ld$us",
		             (const void *)&c,
		             c.name,
		             duration_cast<microseconds>(time_usage).count(),
		             c.awake.count());

		assert(settings.slice_assertion == 0us || time_usage < settings.slice_assertion);
	}

	if(unlikely(settings.slice_interrupt > 0us && time_usage >= settings.slice_interrupt))
		throw interrupted("ctx(%p): Time slice exceeded (last: %06ld microseconds)",
		                  (const void *)&c,
		                  duration_cast<microseconds>(time_usage).count());
}

void
ircd::ctx::prof::check_stack()
{
	auto &c(cur());
	const double &stack_max(c.stack_max);
	const auto stack_usage(stack_usage_here(c));

	if(unlikely(stack_usage > stack_max * settings.stack_usage_warning))
	{
		log::warning("CONTEXT STACK USAGE ctx(%p) used %zu of %zu bytes",
		             (const void *)&c,
		             stack_usage,
		             c.stack_max);

		assert(stack_usage < c.stack_max * settings.stack_usage_assertion);
	}
}

size_t
ircd::ctx::prof::stack_usage_here(const ctx &ctx)
{
	return ctx.stack_base - uintptr_t(__builtin_frame_address(0));
}

///////////////////////////////////////////////////////////////////////////////
//
// ctx_ole.h
//

namespace ircd {
namespace ctx  {
namespace ole  {

using closure = std::function<void () noexcept>;

std::mutex mutex;
std::condition_variable cond;
std::deque<closure> queue;
bool interruption;
std::thread *thread;

closure pop();
void worker() noexcept;
void push(closure &&);

} // namespace ole
} // namespace ctx
} // namespace ircd

ircd::ctx::ole::init::init()
{
	assert(!thread);
	interruption = false;
	thread = new std::thread(&worker);
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
	std::exception_ptr eptr;
	auto &context(cur());
	std::atomic<bool> done{false};
	auto closure([&func, &eptr, &context, &done]
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

		done.store(true, std::memory_order_release);
		notify(context);
	});

	push(std::move(closure)); do
	{
		wait();
	}
	while(!done.load(std::memory_order_consume));

	if(eptr)
		std::rethrow_exception(eptr);
}

void
ircd::ctx::ole::push(closure &&func)
{
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
