// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

/// "main" thread for IRCd; the one the main context landed on.
decltype(ircd::ios::main_thread_id)
ircd::ios::main_thread_id;

/// The embedder/executable's (library user) asio::executor provided on init.
decltype(ircd::ios::user)
ircd::ios::user;

/// Our library-specific/isolate executor.
decltype(ircd::ios::main)
ircd::ios::main;

decltype(ircd::boost_version_api)
ircd::boost_version_api
{
	"boost", info::versions::API, BOOST_VERSION,
	{
		BOOST_VERSION / 100000,
		BOOST_VERSION / 100 % 1000,
		BOOST_VERSION % 100,
	}
};

decltype(ircd::boost_version_abi)
ircd::boost_version_abi
{
	"boost", info::versions::ABI //TODO: get this
};

//
// init
//

void
ircd::ios::init(asio::executor &&user)
{
	// Sample the ID of this thread. Since this is the first transfer of
	// control to libircd after static initialization we have nothing to
	// consider a main thread yet. We need something set for many assertions
	// to pass.
	main_thread_id = std::this_thread::get_id();

	// Set a reference to the user's ios_service
	ios::user = std::move(user);

	// (simple passthru for now)
	ios::main = ios::user;
}

//
// descriptor
//

template<>
decltype(ircd::util::instance_list<ircd::ios::descriptor>::allocator)
ircd::util::instance_list<ircd::ios::descriptor>::allocator
{};

template<>
decltype(ircd::util::instance_list<ircd::ios::descriptor>::list)
ircd::util::instance_list<ircd::ios::descriptor>::list
{
	allocator
};

decltype(ircd::ios::descriptor::ids)
ircd::ios::descriptor::ids;

//
// descriptor::descriptor
//

ircd::ios::descriptor::descriptor(const string_view &name,
                                  const decltype(allocator) &allocator,
                                  const decltype(deallocator) &deallocator,
                                  const bool &continuation)
:name
{
	name
}
,stats
{
	std::make_unique<struct stats>()
}
,allocator
{
	allocator?: default_allocator
}
,deallocator
{
	deallocator?: default_deallocator
}
,history
(
	256, {0}
)
,history_pos
{
	0
}
,continuation
{
	continuation
}
{
}

ircd::ios::descriptor::~descriptor()
noexcept
{
}

[[gnu::hot]]
void
ircd::ios::descriptor::default_deallocator(handler &handler,
                                           void *const &ptr,
                                           const size_t &size)
noexcept
{
	#ifdef __clang__
	::operator delete(ptr);
	#else
	::operator delete(ptr, size);
	#endif
}

[[gnu::hot]]
void *
ircd::ios::descriptor::default_allocator(handler &handler,
                                         const size_t &size)
{
	return ::operator new(size);
}

//
// descriptor::stats
//

ircd::ios::descriptor::stats::stats()
{
}

ircd::ios::descriptor::stats::~stats()
noexcept
{
}

struct ircd::ios::descriptor::stats &
ircd::ios::descriptor::stats::operator+=(const stats &o)
&
{
	queued += o.queued;
	calls += o.calls;
	faults += o.faults;
	allocs += o.allocs;
	alloc_bytes += o.alloc_bytes;
	frees += o.frees;
	free_bytes += o.free_bytes;
	slice_total += o.slice_total;
	slice_last += o.slice_last;
	latency_total += o.latency_total;
	latency_last += o.latency_last;
	return *this;
}

//
// handler
//

namespace ircd::ios
{
	constexpr bool profile_history {false};
}

decltype(ircd::ios::handler::current)
thread_local
ircd::ios::handler::current;

decltype(ircd::ios::handler::epoch)
thread_local
ircd::ios::handler::epoch;

[[gnu::cold]]
bool
ircd::ios::handler::fault(handler *const &handler)
noexcept
{
	assert(handler && handler->descriptor);
	auto &descriptor(*handler->descriptor);

	assert(descriptor.stats);
	auto &stats(*descriptor.stats);
	++stats.faults;

	bool ret(false);
	// leave() isn't called if we return false so the tsc counter
	// needs to be tied off here instead.
	if(!ret)
	{
		const auto last_ts
		{
			std::exchange(handler->ts, cycles())
		};

		assert(handler->ts >= last_ts);
		stats.slice_last = handler->ts - last_ts;
		stats.slice_total += stats.slice_last;

		assert(handler::current == handler);
		handler::current = nullptr;
	}

	return ret;
}

[[gnu::hot]]
void
ircd::ios::handler::leave(handler *const &handler)
noexcept
{
	assert(handler && handler->descriptor);
	auto &descriptor(*handler->descriptor);

	assert(descriptor.stats);
	auto &stats(*descriptor.stats);

	const auto slice_start
	{
		std::exchange(handler->ts, cycles())
	};

	// NOTE: will fail without constant_tsc;
	// NOTE: may fail without nonstop_tsc after OS suspend mode
	assert(handler->ts >= slice_start);
	stats.slice_last = handler->ts - slice_start;
	stats.slice_total += stats.slice_last;

	if constexpr(profile_history)
	{
		assert(descriptor.history_pos < descriptor.history.size());
		descriptor.history[descriptor.history_pos][0] = handler::epoch;
		descriptor.history[descriptor.history_pos][1] = stats.slice_last;
		++descriptor.history_pos;
	}

	assert(handler::current == handler);
	handler::current = nullptr;
}

[[gnu::hot]]
void
ircd::ios::handler::enter(handler *const &handler)
noexcept
{
	assert(!handler::current);
	handler::current = handler;
	++handler::epoch;

	assert(handler && handler->descriptor);
	auto &descriptor(*handler->descriptor);

	assert(descriptor.stats);
	auto &stats(*descriptor.stats);
	++stats.calls;

	const auto last_ts
	{
		std::exchange(handler->ts, cycles())
	};

	stats.latency_last = handler->ts - last_ts;
	stats.latency_total += stats.latency_last;
}

[[gnu::hot]]
bool
ircd::ios::handler::continuation(handler *const &handler)
noexcept
{
	assert(handler && handler->descriptor);
	auto &descriptor(*handler->descriptor);
	return descriptor.continuation;
}

[[gnu::hot]]
void
ircd::ios::handler::deallocate(handler *const &handler,
                               void *const &ptr,
                               const size_t &size)
noexcept
{
	assert(handler && handler->descriptor);
	auto &descriptor(*handler->descriptor);

	descriptor.deallocator(*handler, ptr, size);

	assert(descriptor.stats);
	auto &stats(*descriptor.stats);
	stats.free_bytes += size;
	++stats.frees;
}

[[gnu::hot]]
void *
ircd::ios::handler::allocate(handler *const &handler,
                             const size_t &size)
{
	assert(handler && handler->descriptor);
	auto &descriptor(*handler->descriptor);

	assert(descriptor.stats);
	auto &stats(*descriptor.stats);
	stats.alloc_bytes += size;
	++stats.allocs;

	return descriptor.allocator(*handler, size);
}

//
// ios.h
//

namespace ircd::ios
{
	extern descriptor post_desc;
	extern descriptor defer_desc;
	extern descriptor dispatch_desc;
}

void
ircd::ios::forking()
{
	#if BOOST_VERSION >= 107000
		get().context().notify_fork(asio::execution_context::fork_prepare);
	#else
		get().notify_fork(asio::execution_context::fork_prepare);
	#endif
}

void
ircd::ios::forked_child()
{
	#if BOOST_VERSION >= 107000
		get().context().notify_fork(asio::execution_context::fork_child);
	#else
		get().notify_fork(asio::execution_context::fork_child);
	#endif
}

void
ircd::ios::forked_parent()
{
	#if BOOST_VERSION >= 107000
		get().context().notify_fork(asio::execution_context::fork_parent);
	#else
		get().notify_fork(asio::execution_context::fork_parent);
	#endif
}

//
// dispatch
//

decltype(ircd::ios::dispatch_desc)
ircd::ios::dispatch_desc
{
	"ircd::ios dispatch"
};

[[gnu::hot]]
ircd::ios::dispatch::dispatch(std::function<void ()> function)
:dispatch
{
	dispatch_desc, std::move(function)
}
{
}

ircd::ios::dispatch::dispatch(synchronous_t,
                              const std::function<void ()> &function)
:dispatch
{
	dispatch_desc, synchronous, std::move(function)
}
{
}

ircd::ios::dispatch::dispatch(descriptor &descriptor,
                              synchronous_t)
:dispatch
{
	dispatch_desc, synchronous, []
	{
	}
}
{
}

ircd::ios::dispatch::dispatch(descriptor &descriptor,
                              synchronous_t,
                              const std::function<void ()> &function)
{
	const ctx::uninterruptible::nothrow ui;

	ctx::latch latch(1);
	dispatch(descriptor, [&function, &latch]
	{
		const unwind uw{[&latch]
		{
			latch.count_down();
		}};

		function();
	});

	latch.wait();
}

[[gnu::hot]]
ircd::ios::dispatch::dispatch(descriptor &descriptor,
                              std::function<void ()> function)
{
	boost::asio::dispatch(get(), handle(descriptor, std::move(function)));
}

//
// defer
//

decltype(ircd::ios::defer_desc)
ircd::ios::defer_desc
{
	"ircd::ios defer",
	descriptor::default_allocator,
	descriptor::default_deallocator,
	true, // continuation
};

[[gnu::hot]]
ircd::ios::defer::defer(std::function<void ()> function)
:defer
{
	defer_desc, std::move(function)
}
{
}

ircd::ios::defer::defer(synchronous_t,
                        const std::function<void ()> &function)
:defer
{
	defer_desc, synchronous, function
}
{
}

ircd::ios::defer::defer(descriptor &descriptor,
                        synchronous_t)
:defer
{
	defer_desc, synchronous, []
	{
	}
}
{
}

ircd::ios::defer::defer(descriptor &descriptor,
                        synchronous_t,
                        const std::function<void ()> &function)
{
	const ctx::uninterruptible::nothrow ui;

	ctx::latch latch(1);
	defer(descriptor, [&function, &latch]
	{
		const unwind uw{[&latch]
		{
			latch.count_down();
		}};

		function();
	});

	latch.wait();
}

[[gnu::hot]]
ircd::ios::defer::defer(descriptor &descriptor,
                        std::function<void ()> function)
{
	boost::asio::defer(get(), handle(descriptor, std::move(function)));
}

//
// post
//

decltype(ircd::ios::post_desc)
ircd::ios::post_desc
{
	"ircd::ios post"
};

[[gnu::hot]]
ircd::ios::post::post(std::function<void ()> function)
:post
{
	post_desc, std::move(function)
}
{
}

ircd::ios::post::post(synchronous_t,
                      const std::function<void ()> &function)
:post
{
	post_desc, synchronous, function
}
{
}

ircd::ios::post::post(descriptor &descriptor,
                      synchronous_t)
:post
{
	descriptor, synchronous, []
	{
	}
}
{
}

ircd::ios::post::post(descriptor &descriptor,
                      synchronous_t,
                      const std::function<void ()> &function)
{
	const ctx::uninterruptible::nothrow ui;

	ctx::latch latch(1);
	post(descriptor, [&function, &latch]
	{
		const unwind uw{[&latch]
		{
			latch.count_down();
		}};

		function();
	});

	latch.wait();
}

[[gnu::hot]]
ircd::ios::post::post(descriptor &descriptor,
                      std::function<void ()> function)
{
	boost::asio::post(get(), handle(descriptor, std::move(function)));
}

bool
ircd::ios::available()
noexcept
{
	return bool(main);
}
