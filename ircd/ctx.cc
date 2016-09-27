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

#include <ircd/ctx_ctx.h>

using namespace ircd;

///////////////////////////////////////////////////////////////////////////////
//
// ctx.h
//
__thread ctx::ctx *ctx::current;

void
ctx::sleep_until(const std::chrono::steady_clock::time_point &tp)
{
	while(!wait_until(tp, std::nothrow));
}

bool
ctx::wait_until(const std::chrono::steady_clock::time_point &tp,
                const std::nothrow_t &)
{
	auto &c(cur());
	c.alarm.expires_at(tp);
	c.wait(); // now you're yielding with portals

	return std::chrono::steady_clock::now() >= tp;
}

std::chrono::microseconds
ctx::wait(const std::chrono::microseconds &duration,
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
ctx::wait()
{
	auto &c(cur());
	c.alarm.expires_at(std::chrono::steady_clock::time_point::max());
	c.wait(); // now you're yielding with portals
}

ircd::ctx::context::context(const size_t &stack_sz,
                            std::function<void ()> func,
                            const enum flags &flags)
:c{std::make_unique<ctx>(stack_sz, flags, ircd::ios)}
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

	if(flags & DEFER_POST)
		ios->post(std::move(spawn));
	else if(flags & DEFER_DISPATCH)
		ios->dispatch(std::move(spawn));
	else
		spawn();

	if(flags & SELF_DESTRUCT)
		c.release();
}

ircd::ctx::context::context(std::function<void ()> func,
                            const enum flags &flags)
:context
{
	DEFAULT_STACK_SIZE,
	std::move(func),
	flags
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
	return c.release();
}

bool
ircd::ctx::notify(ctx &ctx)
{
	return ctx.note();
}

void
ircd::ctx::interrupt(ctx &ctx)
{
	ctx.flags |= INTERRUPTED;
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

const enum ctx::flags &
ircd::ctx::flags(const ctx &ctx)
{
	return ctx.flags;
}

const int64_t &
ircd::ctx::notes(const ctx &ctx)
{
	return ctx.notes;
}

///////////////////////////////////////////////////////////////////////////////
//
// ctx_ctx.h
//

ctx::ctx::ctx(const size_t &stack_max,
              const enum flags &flags,
              boost::asio::io_service *const &ios)
:alarm{*ios}
,yc{nullptr}
,stack_base{0}
,stack_max{stack_max}
,notes{1}
,adjoindre{nullptr}
,flags{flags}
{
}

void
ctx::ctx::operator()(boost::asio::yield_context yc,
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
		if(adjoindre)
			notify(*adjoindre);

		mark(prof::event::CUR_LEAVE);
		ircd::ctx::current = nullptr;
		this->yc = nullptr;

		if(flags & SELF_DESTRUCT)
			delete this;
	});

	if(likely(bool(func)))
		func();
}

///////////////////////////////////////////////////////////////////////////////
//
// ctx_pool.h
//

ctx::pool::pool(const size_t &size,
                const size_t &stack_size)
:stack_size{stack_size}
,available{0}
{
	add(size);
}

ctx::pool::~pool()
noexcept
{
	del(size());
}

void
ctx::pool::operator()(closure closure)
{
	queue.emplace_back(std::move(closure));
	dock.notify_one();
}

void
ctx::pool::del(const size_t &num)
{
	const ssize_t requested(size() - num);
	const size_t target(std::max(requested, ssize_t(0)));
	while(ctxs.size() > target)
		ctxs.pop_back();
}

void
ctx::pool::add(const size_t &num)
{
	for(size_t i(0); i < num; ++i)
		ctxs.emplace_back(stack_size, std::bind(&pool::main, this), DEFER_POST);
}

void
ctx::pool::main()
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
ctx::pool::next()
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
	0.66,        // stack_usage_warning
	0.87,        // stack_usage_assertion

	5000us,      // slice_warning
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
ctx::prof::mark(const event &e)
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
ctx::prof::handle_cur_enter()
{
	slice_start();
}

void
ctx::prof::handle_cur_leave()
{
	check_slice();
}

void
ctx::prof::handle_cur_yield()
{
	check_stack();
	check_slice();
}

void
ctx::prof::handle_cur_continue()
{
	slice_start();
}

void
ctx::prof::slice_start()
{
	cur_slice_start = steady_clock::now();
}

void
ctx::prof::check_slice()
{
	auto &c(cur());
	const auto time_usage(steady_clock::now() - cur_slice_start);
	if(unlikely(settings.slice_warning > 0us && time_usage >= settings.slice_warning))
	{
		log::warning("CONTEXT TIMESLICE EXCEEDED ctx(%p) last: %06ld microseconds",
		             (const void *)&c,
		             duration_cast<microseconds>(time_usage).count());

		assert(settings.slice_assertion == 0us || time_usage < settings.slice_assertion);
	}

	if(unlikely(settings.slice_interrupt > 0us && time_usage >= settings.slice_interrupt))
		throw interrupted("ctx(%p): Time slice exceeded (last: %06ld microseconds)",
		                  (const void *)&c,
		                  duration_cast<microseconds>(time_usage).count());
}

void
ctx::prof::check_stack()
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
ctx::prof::stack_usage_here(const ctx &ctx)
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

ctx::ole::init::init()
{
	assert(!thread);
	interruption = false;
	thread = new std::thread(&worker);
}

ctx::ole::init::~init()
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
ctx::ole::offload(const std::function<void ()> &func)
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
ctx::ole::push(closure &&func)
{
	const std::lock_guard<decltype(mutex)> lock(mutex);
	queue.emplace_back(std::move(func));
	cond.notify_one();
}

void
ctx::ole::worker()
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

ctx::ole::closure
ctx::ole::pop()
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
