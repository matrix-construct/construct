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
	struct io_context;
	struct signal_set;

	template<class function>
	void asio_handler_invoke(function&, ...);
}

namespace ircd
{
	namespace asio = boost::asio;      ///< Alias so that asio:: can be used.

	extern const uint boost_version[3];
	extern const string_view boost_version_str;
}

namespace ircd::ios
{
	struct handler;
	struct descriptor;
	template<class function> struct handle;

	extern const std::thread::id static_thread_id;
	extern std::thread::id main_thread_id;
	extern asio::io_context *user;

	bool is_main_thread();
	bool is_static_thread();
	void assert_main_thread();

	bool available();
	asio::io_context &get();

	const string_view &name(const descriptor &);
	const string_view &name(const handler &);

	void dispatch(descriptor &, std::function<void ()>);
	void post(descriptor &, std::function<void ()>);
	void dispatch(std::function<void ()>);
	void post(std::function<void ()>);
	void init(asio::io_context &user);
}

namespace ircd
{
	using ios::assert_main_thread;
	using ios::is_main_thread;
	using ios::dispatch;
	using ios::post;
}

struct ircd::ios::descriptor
:instance_list<descriptor>
{
	struct stats;

	static uint64_t ids;

	static void *default_allocator(handler &, const size_t &);
	static void default_deallocator(handler &, void *const &, const size_t &);

	string_view name;
	uint64_t id {++ids};
	std::unique_ptr<struct stats> stats;
	std::function<void *(handler &, const size_t &)> allocator;
	std::function<void (handler &, void *const &, const size_t &)> deallocator;
	bool continuation;

	descriptor(const string_view &name,
	           const decltype(allocator) & = default_allocator,
	           const decltype(deallocator) & = default_deallocator,
	           const bool &continuation = false);

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

	stats &operator+=(const stats &) &;

	stats();
	~stats() noexcept;
};

struct ircd::ios::handler
{
	static thread_local handler *current;

	static void *allocate(handler *const &, const size_t &);
	static void deallocate(handler *const &, void *const &, const size_t &);
	static bool continuation(handler *const &);
	static void enter(handler *const &);
	static void leave(handler *const &);
	static bool fault(handler *const &);

	ios::descriptor *descriptor;
	uint64_t slice_start {0};
};

template<class function>
struct ircd::ios::handle
:handler
{
	function f;

	template<class... args>
	void operator()(args&&... a) const;

	handle(ios::descriptor &d, function&& f);
};

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

template<class function>
ircd::ios::handle<function>::handle(ios::descriptor &d,
                                    function&& f)
:handler{&d}
,f{std::forward<function>(f)}
{
	assert(d.stats);
	d.stats->queued++;
}

template<class function>
template<class... args>
void
ircd::ios::handle<function>::operator()(args&&... a)
const
{
	assert(descriptor && descriptor->stats);
	assert(descriptor->stats->queued > 0);
	descriptor->stats->queued--;
	f(std::forward<args>(a)...);
}

template<class callable,
         class function>
void
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
bool
ircd::ios::asio_handler_is_continuation(handle<function> *const h)
{
	return handler::continuation(h);
}

template<class function>
void *
__attribute__((malloc, returns_nonnull, warn_unused_result, alloc_size(1)))
ircd::ios::asio_handler_allocate(size_t size,
                                 handle<function> *const h)
{
	return handler::allocate(h, size);
}

template<class function>
void
ircd::ios::asio_handler_deallocate(void *const ptr,
                                   size_t size,
                                   handle<function> *const h)
{
	handler::deallocate(h, ptr, size);
}

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

inline void
ircd::ios::assert_main_thread()
{
	assert(is_main_thread());
}

inline bool
ircd::ios::is_main_thread()
{
	return std::this_thread::get_id() == main_thread_id;
}

inline bool
ircd::ios::is_static_thread()
{
	return std::this_thread::get_id() == static_thread_id;
}
