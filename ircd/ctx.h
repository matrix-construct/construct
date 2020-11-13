// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

/// We make a special use of the stack-protector canary in certain places as
/// another tool to detect corruption of a context's stack, specifically
/// during yield and resume. This use is not really to provide security; just
/// a kind of extra assertion, so we eliminate its emission during release.
#if !defined(NDEBUG) && !defined(__clang__)
	#define IRCD_CTX_STACK_PROTECT __attribute__((stack_protect))
#else
	#define IRCD_CTX_STACK_PROTECT
#endif

namespace ircd::ctx::prof
{
	static void mark(const event &);
}

/// Internal context implementation
///
struct ircd::ctx::ctx
:instance_list<ctx>
{
	using flags_type = std::underlying_type<context::flags>::type;

	static uint64_t id_ctr;                      // monotonic
	static ios::descriptor ios_desc;
	static ios::handler ios_handler;
	static ctx *spawning;
	static dock adjoindre;                       // contexts waiting for join

	uint64_t id {++id_ctr};                      // Unique runtime ID
	char name[16] {0};                           // User given name
	flags_type flags;                            // User given flags
	int8_t nice {0};                             // Scheduling priority nice-value
	int8_t ionice {0};                           // IO priority nice-value (defaults for fs::opts)
	int32_t notes {0};                           // norm: 0 = asleep; 1 = awake; inc by others; dec by self
	boost::asio::deadline_timer alarm;           // acting semaphore (64B)
	boost::asio::yield_context *yc {nullptr};    // boost interface
	continuation *cont {nullptr};                // valid when asleep; invalid when awake
	list::node node;                             // node for ctx::list
	ircd::ctx::stack stack;                      // stack related structure
	prof::ticker profile;                        // prof related structure

	bool started() const noexcept;               // context was ever entered
	bool finished() const noexcept;              // context will not be further entered.
	bool interruption() const noexcept;

	bool interruption_point(std::nothrow_t) noexcept;
	bool termination_point(std::nothrow_t) noexcept;
	void interruption_point();

	bool wake() noexcept;                        // jump to context by queueing with ios (use note())
	bool note() noexcept;                        // properly request wake()
	bool wait();                                 // yield context to ios queue (returns on this resume)
	void jump();                                 // jump to context directly (returns on your resume)

	void operator()(boost::asio::yield_context, const std::function<void ()>) noexcept;
	void spawn(context::function func);

	ctx(const string_view &name     = "<noname>"_sv,
	    const ircd::ctx::stack &    = mutable_buffer{nullptr, DEFAULT_STACK_SIZE},
	    const context::flags &      = (context::flags)0U);

	ctx(ctx &&) = delete;
	ctx(const ctx &) = delete;
	ctx &operator=(ctx &&) = delete;
	ctx &operator=(const ctx &) = delete;
	~ctx() noexcept;
};
