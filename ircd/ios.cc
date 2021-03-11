// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

/// Logging facility
decltype(ircd::ios::log)
ircd::ios::log
{
	"ios"
};

/// "main" thread for IRCd; the one the main context landed on.
decltype(ircd::ios::main_thread_id)
ircd::ios::main_thread_id
{
	std::this_thread::get_id()
};

/// True only for the main thread.
decltype(ircd::ios::is_main_thread)
thread_local
ircd::ios::is_main_thread;

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

void
ircd::ios::init(asio::executor &&user)
{
	// Sample the ID of this thread. Since this is the first transfer of
	// control to libircd after static initialization we have nothing to
	// consider a main thread yet. We need something set for many assertions
	// to pass.
	main_thread_id = std::this_thread::get_id();

	// Set the thread-local bit to true; all other threads will see false.
	is_main_thread = true;

	// Set a reference to the user's ios_service
	ios::user = std::move(user);

	// (simple passthru for now)
	ios::main = ios::user;
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

bool
ircd::ios::available()
noexcept
{
	return bool(main);
}

//
// emption
//

namespace ircd::ios::empt
{
	[[gnu::visibility("internal")]]
	extern const string_view freq_help;

	[[gnu::visibility("internal")]]
	extern uint64_t stats[9];
}

decltype(ircd::ios::empt::freq_help)
ircd::ios::empt::freq_help
{R"(
	Coarse frequency to make non-blocking polls to the kernel for events at the
	beginning of every iteration of the core event loop. boost::asio takes an
	opportunity to first make a non-blocking poll to gather more events from
	the kernel even when one or more tasks are already queued, this setting
	allows more such tasks to first be executed and reduce syscall overhead
	ncluding a large numbers of unnecessary calls as would be the case
	otherwise.

	When the frequency is set to 1, the above-described default behavior is
	unaltered. When greater than 1, voluntary non-blocking polls are only made
	after N number of tasks. This reduces syscalls to increase overall
	performance, but may cost in responsiveness and cause stalls. For example,
	when set to 2, kernel context-switch is made every other userspace context
	switch. When set to 0, voluntary non-blocking polls are never made.

	This value may be rounded down to nearest base2 so we can avoid invoking
	the FPU in the core event loop's codepath.
)"};

decltype(ircd::ios::empt::stats)
ircd::ios::empt::stats;

/// Voluntary kernel poll frequency.
decltype(ircd::ios::empt::freq)
ircd::ios::empt::freq
{
	{ "name",      "ircd.ios.empt.freq" },
	{ "default",   512                  },
	{ "help",      freq_help            },
};

/// Non-blocking call count.
decltype(ircd::ios::empt::peek)
ircd::ios::empt::peek
{
	stats + 0,
	{
		{ "name", "ircd.ios.empt.peek" },
	},
};

/// Skipped call count.
decltype(ircd::ios::empt::skip)
ircd::ios::empt::skip
{
	stats + 1,
	{
		{ "name", "ircd.ios.empt.skip" },
	},
};

/// Non-skipped call count.
decltype(ircd::ios::empt::call)
ircd::ios::empt::call
{
	stats + 2,
	{
		{ "name", "ircd.ios.empt.call" },
	},
};

/// Count of calls which reported zero ready events.
decltype(ircd::ios::empt::none)
ircd::ios::empt::none
{
	stats + 3,
	{
		{ "name", "ircd.ios.empt.none" },
	},
};

/// Total number of events reported from all calls.
decltype(ircd::ios::empt::result)
ircd::ios::empt::result
{
	stats + 4,
	{
		{ "name", "ircd.ios.empt.result" },
	},
};

/// Count of calls which reported more events than the low threshold.
decltype(ircd::ios::empt::load_low)
ircd::ios::empt::load_low
{
	stats + 5,
	{
		{ "name", "ircd.ios.empt.load.low" },
	},
};

/// Count of calls which reported more events than the medium threshold.
decltype(ircd::ios::empt::load_med)
ircd::ios::empt::load_med
{
	stats + 6,
	{
		{ "name", "ircd.ios.empt.load.med" },
	},
};

/// Count of calls which reported more events than the high threshold.
decltype(ircd::ios::empt::load_high)
ircd::ios::empt::load_high
{
	stats + 7,
	{
		{ "name", "ircd.ios.empt.load.high" },
	},
};

/// Count of calls which reported the maximum number of events.
decltype(ircd::ios::empt::load_stall)
ircd::ios::empt::load_stall
{
	stats + 8,
	{
		{ "name", "ircd.ios.empt.load.stall" },
	}
};

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
noexcept
:name
{
	name
}
,stats
{
	std::make_unique<struct stats>(*this)
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
	assert(!stats || stats->queued == 0);
	assert(!stats || stats->allocs == stats->frees);
	assert(!stats || stats->alloc_bytes == stats->free_bytes);
}

//
// descriptor::stats
//

namespace ircd::ios
{
	static thread_local char stats_name_buf[128];
	static string_view stats_name(const descriptor &d, const string_view &key);
}

ircd::string_view
ircd::ios::stats_name(const descriptor &d,
                      const string_view &key)
{
	return fmt::sprintf
	{
		stats_name_buf, "ircd.ios.%s.%s",
		d.name,
		key,
	};
}

ircd::ios::descriptor::stats::stats(descriptor &d)
:value{0}
,items{0}
,queued
{
	value + items++,
	{
		{ "name", stats_name(d, "queued") },
	},
}
,calls
{
	value + items++,
	{
		{ "name", stats_name(d, "calls") },
	},
}
,faults
{
	value + items++,
	{
		{ "name", stats_name(d, "faults") },
	},
}
,allocs
{
	value + items++,
	{
		{ "name", stats_name(d, "allocs") },
	},
}
,alloc_bytes
{
	value + items++,
	{
		{ "name", stats_name(d, "alloc_bytes") },
	},
}
,frees
{
	value + items++,
	{
		{ "name", stats_name(d, "frees") },
	},
}
,free_bytes
{
	value + items++,
	{
		{ "name", stats_name(d, "free_bytes") },
	},
}
,slice_total
{
	value + items++,
	{
		{ "name", stats_name(d, "slice_total") },
	},
}
,slice_last
{
	value + items++,
	{
		{ "name", stats_name(d, "slice_last") },
	},
}
,latency_total
{
	value + items++,
	{
		{ "name", stats_name(d, "latency_total") },
	},
}
,latency_last
{
	value + items++,
	{
		{ "name", stats_name(d, "latency_last") },
	},
}
{
	assert(items <= (sizeof(value) / sizeof(value[0])));
}

ircd::ios::descriptor::stats::~stats()
noexcept
{
}

//
// handler
//

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

	bool ret
	{
		false
	};

	if constexpr(profile::logging)
		log::logf
		{
			log, log::level::DEBUG,
			"FAULT %5u %-30s [%11lu] faults[%9lu] q:%-4lu",
			descriptor.id,
			trunc(descriptor.name, 30),
			uint64_t(stats.calls),
			uint64_t(stats.faults),
			uint64_t(stats.queued),
		};

	// Our API sez if this function returns true, caller is responsible for
	// calling leave(), otherwise they must not call leave().
	if(likely(!ret))
		leave(handler);

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

	if constexpr(profile::history)
	{
		assert(descriptor.history_pos < descriptor.history.size());
		descriptor.history[descriptor.history_pos][0] = handler::epoch;
		descriptor.history[descriptor.history_pos][1] = stats.slice_last;
		++descriptor.history_pos;
	}

	if constexpr(profile::logging)
		log::logf
		{
			log, log::level::DEBUG,
			"LEAVE %5u %-30s [%11lu] cycles[%9lu] q:%-4lu",
			descriptor.id,
			trunc(descriptor.name, 30),
			uint64_t(stats.calls),
			uint64_t(stats.slice_last),
			uint64_t(stats.queued),
		};

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

	if constexpr(profile::logging)
		log::logf
		{
			log, log::level::DEBUG,
			"ENTER %5u %-30s [%11lu] latent[%9lu] q:%-4lu",
			descriptor.id,
			trunc(descriptor.name, 30),
			uint64_t(stats.calls),
			uint64_t(stats.latency_last),
			uint64_t(stats.queued),
		};
}

//
// dispatch
//

ircd::ios::dispatch::dispatch(descriptor &descriptor,
                              defer_t,
                              yield_t)
:dispatch
{
	descriptor, defer, yield, []
	{
	}
}
{
}

ircd::ios::dispatch::dispatch(descriptor &descriptor,
                              defer_t,
                              yield_t,
                              const std::function<void ()> &function)
{
	const ctx::uninterruptible::nothrow ui;
	ctx::latch latch{1};
	dispatch
	{
		descriptor, defer, [&function, &latch]
		{
			const unwind uw
			{
				[&latch]
				{
					latch.count_down();
				}
			};

			function();
		}
	};

	latch.wait();
}

[[gnu::hot]]
ircd::ios::dispatch::dispatch(descriptor &descriptor,
                              defer_t,
                              std::function<void ()> function)
{
	boost::asio::post(get(), handle(descriptor, std::move(function)));
}

ircd::ios::dispatch::dispatch(descriptor &descriptor,
                              yield_t,
                              const std::function<void ()> &function)
{
	assert(function);
	assert(ctx::current && handler::current);

	const ctx::uninterruptible ui;
	ctx::continuation
	{
		continuation::false_predicate, continuation::noop_interruptor, [&descriptor, &function]
		(auto &yield)
		{
			assert(!ctx::current && !handler::current);
			boost::asio::dispatch(get(), handle(descriptor, function));

			assert(!ctx::current && !handler::current);
		}
	};
}

[[gnu::hot]]
ircd::ios::dispatch::dispatch(descriptor &descriptor,
                              std::function<void ()> function)
{
	const auto parent(handler::current); try
	{
		assert(!ctx::current && handler::current);
		ios::handler::leave(parent);

		assert(!ctx::current && !handler::current);
		boost::asio::dispatch(get(), handle(descriptor, std::move(function)));

		assert(!ctx::current && !handler::current);
		ios::handler::enter(parent);
	}
	catch(...)
	{
		assert(!ctx::current && !handler::current);
		ios::handler::enter(parent);

		assert(!ctx::current && handler::current == parent);
		throw;
	}

	assert(!ctx::current && handler::current == parent);
}
