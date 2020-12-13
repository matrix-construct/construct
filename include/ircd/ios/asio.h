// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_IOS_ASIO_H

/// Forward declarations for boost::asio because it is not included here.
namespace boost::asio
{
	struct io_context;

	template<class function>
	void asio_handler_invoke(function&, ...);
}

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
