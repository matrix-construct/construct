// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <ircd/asio.h>
#include "ctx.h"

namespace ircd::ctx::ole
{
	extern conf::item<size_t> thread_max;
	std::mutex mutex;
	std::condition_variable cond;
	std::deque<offload::function> queue;
	std::vector<std::thread> threads;
	bool termination;

	static offload::function pop();
	static void push(offload::function &&);
	static void worker() noexcept;
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
	std::unique_lock lock
	{
		mutex
	};

	termination = true;
	cond.notify_all();
	cond.wait(lock, []
	{
		return threads.empty();
	});
}

ircd::ctx::ole::offload::offload(const function &func)
:offload{opts{}, func}
{
}

ircd::ctx::ole::offload::offload(const opts &opts,
                                 const function &func)
{
	assert(current);
	assert(opts.concurrency == 1); // not yet implemented

	// Prepare the offload package on our stack here. These objects will
	// remain here for the duration of the offload.
	latch latch{1};
	std::exception_ptr eptr;
	auto *const context(current);
	auto closure{[&func, &latch, &eptr, &context]
	() noexcept
	{
		try
		{
			func();
		}
		catch(...)
		{
			// Note that the write to eptr is taking place on a different
			// thread from where we created the eptr.
			eptr = std::current_exception();
		}

		// The ctx::signal() is a special device which executes the closure
		// as soon as the target context is not currently running on any
		// thread. This has the ability to provide the cross-thread
		// synchronization we need to hit the latch from this thread.
		assert(context);
		signal(*context, [&latch]
		{
			assert(!latch.is_ready());
			latch.count_down();
		});
	}};

	// interrupt(ctx) is suppressed while this context has offloaded some work
	// to another thread. This context must stay right here and not disappear
	// until the other thread signals back. Note that the destructor is
	// capable of throwing an interrupt that was received during this scope.
	const uninterruptible uninterruptible;

	ole::push(std::move(closure));       // scope address required for clang-7
	latch.wait();

	// Don't throw any exception if there is a pending interrupt for this ctx.
	// Two exceptions will be thrown in that case and if there's an interrupt
	// we don't care about eptr anyway.
	if(likely(!interruption_requested()))
		if(unlikely(eptr))
			std::rethrow_exception(eptr);
}
void
ircd::ctx::ole::push(offload::function &&func)
{
	if(unlikely(threads.size() < size_t(thread_max)))
		threads.emplace_back(&worker);

	const std::lock_guard lock
	{
		mutex
	};

	queue.emplace_back(std::move(func));
	cond.notify_all();
}

void
ircd::ctx::ole::worker()
noexcept try
{
	while(1)
	{
		const auto func
		{
			pop()
		};

		func();
	}
}
catch(const interrupted &)
{
	const std::lock_guard lock
	{
		mutex
	};

	const auto it
	{
		std::find_if(begin(threads), end(threads), []
		(const auto &thread)
		{
			return thread.get_id() == std::this_thread::get_id();
		})
	};

	assert(it != end(threads));
	auto &this_thread(*it);
	this_thread.detach();
	threads.erase(it);
	cond.notify_all();
}

ircd::ctx::ole::offload::function
ircd::ctx::ole::pop()
{
	std::unique_lock lock
	{
		mutex
	};

	cond.wait(lock, []
	{
		if(!queue.empty())
			return true;

		if(unlikely(termination))
			throw interrupted{};

		return false;
	});

	auto function
	{
		std::move(queue.front())
	};

	queue.pop_front();
	return function;
}
