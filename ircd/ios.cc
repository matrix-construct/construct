// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <ircd/asio.h>

/// Boost version indicator for compiled header files.
decltype(ircd::boost_version)
ircd::boost_version
{
	BOOST_VERSION / 100000,
	BOOST_VERSION / 100 % 1000,
	BOOST_VERSION % 100,
};

char ircd_boost_version_str_buf[32];
decltype(ircd::boost_version_str)
ircd::boost_version_str
(
	ircd_boost_version_str_buf,
	::snprintf(ircd_boost_version_str_buf, sizeof(ircd_boost_version_str_buf),
	           "%u.%u.%u",
	           boost_version[0],
	           boost_version[1],
	           boost_version[2])
);

/// Record of the ID of the thread static initialization took place on.
decltype(ircd::ios::static_thread_id)
ircd::ios::static_thread_id
{
    std::this_thread::get_id()
};

/// "main" thread for IRCd; the one the main context landed on.
decltype(ircd::ios::main_thread_id)
ircd::ios::main_thread_id;

decltype(ircd::ios::user)
ircd::ios::user;

void
ircd::ios::init(asio::io_context &user)
{
	// Sample the ID of this thread. Since this is the first transfer of
	// control to libircd after static initialization we have nothing to
	// consider a main thread yet. We need something set for many assertions
	// to pass until ircd::main() is entered which will reset this to where
	// ios.run() is really running.
	main_thread_id = std::this_thread::get_id();

	// Set a reference to the user's ios_service
	ios::user = &user;
}

void
ircd::ios::post(std::function<void ()> function)
{
	static descriptor descriptor
	{
		"ircd::ios post"
	};

	post(descriptor, std::move(function));
}

void
ircd::ios::dispatch(std::function<void ()> function)
{
	static descriptor descriptor
	{
		"ircd::ios dispatch"
	};

	dispatch(descriptor, std::move(function));
}

void
ircd::ios::post(descriptor &descriptor,
                std::function<void ()> function)
{
	boost::asio::post(get(), handle(descriptor, std::move(function)));
}

void
ircd::ios::dispatch(descriptor &descriptor,
                    std::function<void ()> function)
{
	boost::asio::dispatch(get(), handle(descriptor, std::move(function)));
}

boost::asio::io_context &
ircd::ios::get()
{
	assert(user);
	return *user;
}

bool
ircd::ios::available()
{
	return bool(user);
}

//
// descriptor
//

template<>
decltype(ircd::util::instance_list<ircd::ios::descriptor>::list)
ircd::util::instance_list<ircd::ios::descriptor>::list
{};

decltype(ircd::ios::descriptor::ids)
ircd::ios::descriptor::ids;

//
// descriptor::descriptor
//

ircd::ios::descriptor::descriptor(const string_view &name,
                                  const decltype(allocator) &allocator,
                                  const decltype(deallocator) &deallocator)
:name{name}
,allocator{allocator}
,deallocator{deallocator}
{
	assert(allocator);
	assert(deallocator);
}

ircd::ios::descriptor::~descriptor()
noexcept
{
}

void
ircd::ios::descriptor::default_deallocator(handler &handler,
                                           void *const &ptr,
                                           const size_t &size)
{
	::operator delete(ptr, size);
}

void *
ircd::ios::descriptor::default_allocator(handler &handler,
                                         const size_t &size)
{
	return ::operator new(size);
}

//
// handler
//

decltype(ircd::ios::handler::current) thread_local
ircd::ios::handler::current;

bool
ircd::ios::handler::fault(handler *const &handler)
{
	assert(handler && handler->descriptor);
	auto &descriptor(*handler->descriptor);
	++descriptor.faults;
	bool ret(false);

	// leave() isn't called if we return false so the tsc counter
	// needs to be tied off here instead.
	if(!ret)
	{
		descriptor.slice_last = cycles() - handler->slice_start;
		descriptor.slice_total += descriptor.slice_last;
	}

	return ret;
}

void
ircd::ios::handler::leave(handler *const &handler)
{
	assert(handler && handler->descriptor);
	auto &descriptor(*handler->descriptor);
	descriptor.slice_last = cycles() - handler->slice_start;
	descriptor.slice_total += descriptor.slice_last;
	assert(handler::current == handler);
	handler::current = nullptr;
}

void
ircd::ios::handler::enter(handler *const &handler)
{
	assert(handler && handler->descriptor);
	auto &descriptor(*handler->descriptor);
	++descriptor.calls;
	assert(!handler::current);
	handler::current = handler;
	handler->slice_start = cycles();
}

void
ircd::ios::handler::deallocate(handler *const &handler,
                               void *const &ptr,
                               const size_t &size)
{
	assert(handler && handler->descriptor);
	auto &descriptor(*handler->descriptor);
	descriptor.deallocator(*handler, ptr, size);
	descriptor.free_bytes += size;
	++descriptor.frees;
}

void *
ircd::ios::handler::allocate(handler *const &handler,
                             const size_t &size)
{
	assert(handler && handler->descriptor);
	auto &descriptor(*handler->descriptor);
	descriptor.alloc_bytes += size;
	++descriptor.allocs;
	return descriptor.allocator(*handler, size);
}
