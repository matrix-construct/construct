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
	bool termination;
	std::deque<offload::closure> queue;
	std::vector<std::thread> threads;

	offload::closure pop();
	void push(offload::closure &&);
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
	std::unique_lock lock(mutex);
	termination = true;
	cond.notify_all();
	cond.wait(lock, []
	{
		return threads.empty();
	});
}

ircd::ctx::ole::offload::offload(const opts &opts,
                                 const closure &func)
{
	assert(opts.concurrency == 1); // not yet implemented

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
	//
	// Don't throw any exception if there is a pending interrupt for this ctx.
	// Two exceptions will be thrown in that case and if there's an interrupt
	// we don't care about eptr anyway.
	const uninterruptible uninterruptible;

	push(std::move(closure)); do
	{
		wait();
	}
	while(!done);

	if(unlikely(interruption_requested()))
		return;

	if(eptr)
		std::rethrow_exception(eptr);
}
void
ircd::ctx::ole::push(offload::closure &&func)
{
	if(unlikely(threads.size() < size_t(thread_max)))
		threads.emplace_back(&worker);

	const std::lock_guard lock(mutex);
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
	std::unique_lock lock(mutex);
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

ircd::ctx::ole::offload::closure
ircd::ctx::ole::pop()
{
	std::unique_lock lock(mutex);
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
	return c;
}
