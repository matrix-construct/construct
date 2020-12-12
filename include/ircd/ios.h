// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_IOS_H

// Boost headers are not exposed to our users unless explicitly included by a
// definition file. Other libircd headers may extend this namespace with more
// forward declarations.

/// Forward declarations for boost::asio because it is not included here.
namespace boost::asio
{
	struct executor;
	struct io_context;

	template<class function>
	void asio_handler_invoke(function&, ...);
}

namespace ircd
{
	namespace asio = boost::asio;      ///< Alias so that asio:: can be used.

	extern const info::versions boost_version_api, boost_version_abi;
}

namespace ircd::ios
{
	struct handler;
	struct descriptor;
	template<class function> struct handle;

	IRCD_OVERLOAD(synchronous)
	struct dispatch;
	struct defer;
	struct post;

	constexpr bool profile_history {false};
	constexpr bool profile_logging {false};

	extern log::log log;
	extern std::thread::id main_thread_id;
	extern asio::executor user;
	extern asio::executor main;

	const string_view &name(const descriptor &);
	const string_view &name(const handler &);

	bool available() noexcept;
	const uint64_t &epoch() noexcept;

	void forked_parent();
	void forked_child();
	void forking();
	void init(asio::executor &&);
}

namespace ircd
{
	using ios::dispatch;
	using ios::defer;
	using ios::post;
}

struct ircd::ios::dispatch
{
	dispatch(descriptor &, std::function<void ()>);
	dispatch(descriptor &, synchronous_t, const std::function<void ()> &);
	dispatch(descriptor &, synchronous_t);
	dispatch(std::function<void ()>);
	dispatch(synchronous_t, const std::function<void ()> &);
};

struct ircd::ios::defer
{
	defer(descriptor &, std::function<void ()>);
	defer(descriptor &, synchronous_t, const std::function<void ()> &);
	defer(descriptor &, synchronous_t);
	defer(std::function<void ()>);
	defer(synchronous_t, const std::function<void ()> &);
};

struct ircd::ios::post
{
	post(descriptor &, std::function<void ()>);
	post(descriptor &, synchronous_t, const std::function<void ()> &);
	post(descriptor &, synchronous_t);
	post(std::function<void ()>);
	post(synchronous_t, const std::function<void ()> &);
};

struct ircd::ios::descriptor
:instance_list<descriptor>
{
	struct stats;

	static uint64_t ids;

	static void *default_allocator(handler &, const size_t &);
	static void default_deallocator(handler &, void *const &, const size_t &) noexcept;

	string_view name;
	uint64_t id {++ids};
	std::unique_ptr<struct stats> stats;
	void *(*allocator)(handler &, const size_t &);
	void (*deallocator)(handler &, void *const &, const size_t &);
	std::vector<std::array<uint64_t, 2>> history; // epoch, cycles
	uint8_t history_pos {0};
	bool continuation {false};

	descriptor(const string_view &name,
	           const decltype(allocator) & = default_allocator,
	           const decltype(deallocator) & = default_deallocator,
	           const bool &continuation = false) noexcept;

	descriptor(descriptor &&) = delete;
	descriptor(const descriptor &) = delete;
	~descriptor() noexcept;
};

struct ircd::ios::descriptor::stats
{
	uint64_t queued {0};
	uint64_t calls {0};
	uint64_t faults {0};
	uint64_t allocs {0};
	uint64_t alloc_bytes{0};
	uint64_t frees {0};
	uint64_t free_bytes{0};
	uint64_t slice_total {0};
	uint64_t slice_last {0};
	uint64_t latency_total {0};
	uint64_t latency_last {0};

	stats &operator+=(const stats &) &;

	stats();
	~stats() noexcept;
};

struct ircd::ios::handler
{
	static thread_local handler *current;
	static thread_local uint64_t epoch;

	static void enqueue(handler *const &) noexcept;
	static void *allocate(handler *const &, const size_t &);
	static void deallocate(handler *const &, void *const &, const size_t &) noexcept;
	static bool continuation(handler *const &) noexcept;
	static void enter(handler *const &) noexcept;
	static void leave(handler *const &) noexcept;
	static bool fault(handler *const &) noexcept;

	ios::descriptor *descriptor {nullptr};
	uint64_t ts {0}; // last tsc sample; for profiling each phase
};

template<class function>
struct ircd::ios::handle
:handler
{
	function f;

	template<class... args>
	void operator()(args&&... a) const;

	handle(ios::descriptor &, function) noexcept;
};

// boost handlers
namespace ircd::ios
{
	template<class function>
	void asio_handler_deallocate(void *, size_t, handle<function> *);

	template<class function>
	void *asio_handler_allocate(size_t, handle<function> *);

	template<class function>
	bool asio_handler_is_continuation(handle<function> *);

	template<class callable,
	         class function>
	void asio_handler_invoke(callable &f, handle<function> *);
}

template<class callable,
         class function>
inline void
ircd::ios::asio_handler_invoke(callable &f,
                               handle<function> *const h)
try
{
	handler::enter(h);
	boost::asio::asio_handler_invoke(f, &h);
	handler::leave(h);
}
catch(...)
{
	if(handler::fault(h))
		handler::leave(h);
	else
		throw;
}

template<class function>
inline bool
ircd::ios::asio_handler_is_continuation(handle<function> *const h)
{
	return handler::continuation(h);
}

template<class function>
inline void *
__attribute__((malloc, returns_nonnull, warn_unused_result, alloc_size(1)))
ircd::ios::asio_handler_allocate(size_t size,
                                 handle<function> *const h)
{
	return handler::allocate(h, size);
}

template<class function>
inline void
ircd::ios::asio_handler_deallocate(void *const ptr,
                                   size_t size,
                                   handle<function> *const h)
{
	handler::deallocate(h, ptr, size);
}

//
// ircd::ios::handle
//

template<class function>
inline
ircd::ios::handle<function>::handle(ios::descriptor &d,
                                    function f)
noexcept
:handler{&d, prof::cycles()}
,f(std::move(f))
{
	handler::enqueue(this);
}

template<class function>
template<class... args>
inline void
ircd::ios::handle<function>::operator()(args&&... a)
const
{
	assert(descriptor && descriptor->stats);
	assert(descriptor->stats->queued > 0);
	descriptor->stats->queued--;
	f(std::forward<args>(a)...);
}

//
// ircd::ios::handler
//

[[gnu::hot]]
inline void
ircd::ios::handler::deallocate(handler *const &handler,
                               void *const &ptr,
                               const size_t &size)
noexcept
{
	assert(handler && handler->descriptor);
	auto &descriptor(*handler->descriptor);

	assert(descriptor.deallocator);
	descriptor.deallocator(*handler, ptr, size);

	assert(descriptor.stats);
	auto &stats(*descriptor.stats);
	stats.free_bytes += size;
	++stats.frees;
}

[[gnu::hot]]
inline void *
ircd::ios::handler::allocate(handler *const &handler,
                             const size_t &size)
{
	assert(handler && handler->descriptor);
	auto &descriptor(*handler->descriptor);

	assert(descriptor.stats);
	auto &stats(*descriptor.stats);
	stats.alloc_bytes += size;
	++stats.allocs;

	assert(descriptor.allocator);
	return descriptor.allocator(*handler, size);
}

[[gnu::hot]]
inline void
ircd::ios::handler::enqueue(handler *const &handler)
noexcept
{
	assert(handler && handler->descriptor);
	auto &descriptor(*handler->descriptor);

	assert(descriptor.stats);
	auto &stats(*descriptor.stats);
	++stats.queued;

	if constexpr(profile_logging)
		log::logf
		{
			log, log::level::DEBUG,
			"QUEUE %5u %-30s [%11lu] ------[%9lu] q:%-4lu",
			descriptor.id,
			trunc(descriptor.name, 30),
			stats.calls,
			0UL,
			stats.queued,
		};
}

[[gnu::hot]]
inline bool
ircd::ios::handler::continuation(handler *const &handler)
noexcept
{
	assert(handler && handler->descriptor);
	auto &descriptor(*handler->descriptor);
	return descriptor.continuation;
}

//
// ios::descriptor
//

[[gnu::hot]]
inline void
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
inline void *
ircd::ios::descriptor::default_allocator(handler &handler,
                                         const size_t &size)
{
	return ::operator new(size);
}

//
// ircd::ios
//

inline const ircd::string_view &
ircd::ios::name(const handler &handler)
{
	assert(handler.descriptor);
	return name(*handler.descriptor);
}

inline const ircd::string_view &
ircd::ios::name(const descriptor &descriptor)
{
	return descriptor.name;
}

inline const uint64_t &
__attribute__((always_inline))
ircd::ios::epoch()
noexcept
{
	return handler::epoch;
}
