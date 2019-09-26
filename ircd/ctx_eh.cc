// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <RB_INC_CXXABI_H
#include "ctx.h"

#ifdef HAVE_CXXABI_H
struct __cxxabiv1::__cxa_eh_globals
{
	__cxa_exception *caughtExceptions;
	unsigned int uncaughtExceptions;
};
#endif

//
// exception_handler
//

ircd::ctx::exception_handler::exception_handler()
noexcept
:std::exception_ptr
{
	// capture a shared reference of the current exception before the catch
	// block closes around it.
	std::current_exception()
}
{
	assert(bool(*this));

	// close the catch block.
	end_catch();

	// We don't yet support more levels of exceptions; after ending this
	// catch we can't still be in another one. This doesn't apply if we're
	// not on any ctx currently.
	assert(!current || !std::uncaught_exceptions());
}

//
// util
//

#ifdef HAVE_CXXABI_H
void
ircd::ctx::exception_handler::end_catch()
noexcept
{
	// Only close the catch block if we're actually on an ircd::ctx. This
	// allows the same codepath with a ctx::exception_handler to be used
	// outside of the ircd::ctx system to not incur any unexpected or
	// potential issues with ending the catch manually.
	if(likely(current))
		__cxxabiv1::__cxa_end_catch();
}
#else
#warning "CXXABI not available. Context switch inside catch block may be unsafe!"
void
ircd::ctx::exception_handler::end_catch()
noexcept
{
}
#endif

#ifdef HAVE_CXXABI_H
/// Get the uncaught exception count
uint
ircd::ctx::exception_handler::uncaught_exceptions()
noexcept
{
	const auto &cxa_globals
	{
		__cxxabiv1::__cxa_get_globals_fast()
	};

	assert(cxa_globals);
	return cxa_globals->uncaughtExceptions;
}
#else
#warning "CXXABI not available. Context switch with uncaught exceptions may be unsafe!"
uint
ircd::ctx::exception_handler::uncaught_exceptions()
noexcept
{
	return std::uncaught_exceptions();
}
#endif

#ifdef HAVE_CXXABI_H
/// Set the uncaught exception count and return the previous value.
uint
ircd::ctx::exception_handler::uncaught_exceptions(const uint &val)
noexcept
{
	const auto &cxa_globals
	{
		__cxxabiv1::__cxa_get_globals_fast()
	};

	assert(cxa_globals);
	const auto ret
	{
		cxa_globals->uncaughtExceptions
	};

	assert(unsigned(ret) == unsigned(std::uncaught_exceptions()));
	cxa_globals->uncaughtExceptions = val;
	return ret;
}
#else
#warning "CXXABI not available. Context switch with uncaught exceptions may be unsafe!"
uint
ircd::ctx::exception_handler::uncaught_exceptions(const uint &val)
noexcept
{
	assert(std::uncaught_exceptions() == val);
	return std::uncaught_exceptions();
}
#endif
