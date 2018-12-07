// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::ctx
{
	struct stack;
	struct profile;

	void spawn(ctx *const c, context::function func);
}

/// Internal structure aggregating any stack related state for the ctx
struct ircd::ctx::stack
{
	uintptr_t base {0};                    // assigned when spawned
	size_t max {0};                        // User given stack size
	size_t at {0};                         // Updated for profiling at sleep

	stack(const size_t &max = 0)
	:max{max}
	{}
};

/// Internal structure aggregating any profiling related state for the ctx
struct ircd::ctx::profile
{
	ulong cycles {0};                            // monotonic counter (rdtsc)
	uint64_t yields {0};                         // monotonic counter
};

/// Internal context implementation
///
struct ircd::ctx::ctx
:instance_list<ctx>
{
	using flags_t = std::underlying_type<context::flags>::type;

	static uint64_t id_ctr;                      // monotonic

	uint64_t id {++id_ctr};                      // Unique runtime ID
	const char *name {nullptr};                  // User given name (optional)
	flags_t flags {0};                           // User given flags
	int32_t notes {0};                           // norm: 0 = asleep; 1 = awake; inc by others; dec by self
	boost::asio::io_service::strand strand;      // mutex/serializer
	boost::asio::steady_timer alarm;             // acting semaphore (64B)
	boost::asio::yield_context *yc {nullptr};    // boost interface
	continuation *cont {nullptr};                // valid when asleep; invalid when awake
	ircd::ctx::stack stack;                      // stack related structure
	ircd::ctx::profile profile;                  // prof related structure
	list::node node;                             // node for ctx::list
	dock adjoindre;                              // contexts waiting for this to join()

	bool started() const;                        // context was ever entered
	bool finished() const;                       // context will not be further entered.

	bool interruption_point(std::nothrow_t);     // Check for interrupt (and clear flag)
	bool termination_point(std::nothrow_t);      // Check for terminate
	void interruption_point();                   // throws interrupted or terminated

	void wake();                                 // jump to context by queueing with ios (use note())
	bool note();                                 // properly request wake()
	void jump();                                 // jump to context directly (returns on your resume)
	bool wait();                                 // yield context to ios queue (returns on this resume)

	void operator()(boost::asio::yield_context, const std::function<void ()>) noexcept;

	ctx(const char *const &name                  = "<noname>",
	    const size_t &stack_max                  = DEFAULT_STACK_SIZE,
	    const context::flags &flags              = (context::flags)0,
	    boost::asio::io_service &ios             = ircd::ios::get())
	:name{name}
	,flags{flags}
	,strand{ios}
	,alarm{ios}
	,stack{stack_max}
	{}

	ctx(ctx &&) = delete;
	ctx(const ctx &) = delete;
	ctx &operator=(ctx &&) = delete;
	ctx &operator=(const ctx &) = delete;
	~ctx() noexcept;
};
