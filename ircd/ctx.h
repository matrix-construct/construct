// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

/// Internal context implementation
///
struct ircd::ctx::ctx
:instance_list<ctx>
{
	using error_code = boost::system::error_code;

	static uint64_t id_ctr;                      // monotonic

	uint64_t id {++id_ctr};                      // Unique runtime ID
	const char *name {nullptr};                  // User given name (optional)
	context::flags flags {(context::flags)0};    // User given flags
	boost::asio::io_service::strand strand;      // mutex/serializer
	boost::asio::steady_timer alarm;             // acting semaphore (64B)
	boost::asio::yield_context *yc {nullptr};    // boost interface
	uintptr_t stack_base {0};                    // assigned when spawned
	size_t stack_max {0};                        // User given stack size
	size_t stack_at {0};                         // Updated for profiling at sleep
	int64_t notes {0};                           // norm: 0 = asleep; 1 = awake; inc by others; dec by self
	ulong cycles {0};                            // monotonic counter (rdtsc)
	uint64_t yields {0};                         // monotonic counter
	continuation *cont {nullptr};                // valid when asleep; invalid when awake
	ctx *adjoindre {nullptr};                    // context waiting for this to join()
	list::node node;                             // node for ctx::list

	bool started() const                         { return stack_base != 0;                         }
	bool finished() const                        { return started() && yc == nullptr;              }

	bool interruption_point(std::nothrow_t);     // Check for interrupt (and clear flag)
	bool termination_point(std::nothrow_t);      // Check for terminate
	void interruption_point();                   // throws interrupted or terminated

	bool wait();                                 // yield context to ios queue (returns on this resume)
	void jump();                                 // jump to context directly (returns on your resume)
	void wake();                                 // jump to context by queueing with ios (use note())
	bool note();                                 // properly request wake()

	void operator()(boost::asio::yield_context, const std::function<void ()>) noexcept;

	ctx(const char *const &name                  = "<noname>",
	    const size_t &stack_max                  = DEFAULT_STACK_SIZE,
	    const context::flags &flags              = (context::flags)0,
	    boost::asio::io_service *const &ios      = ircd::ios)
	:name{name}
	,flags{flags}
	,strand{*ios}
	,alarm{*ios}
	,stack_max{stack_max}
	{}

	ctx(ctx &&) = delete;
	ctx(const ctx &) = delete;
	ctx &operator=(ctx &&) = delete;
	ctx &operator=(const ctx &) = delete;
};
