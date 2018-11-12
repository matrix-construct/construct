// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <signal.h>

#ifndef __GNUC__
#pragma STDC FENV_ACCESS on
#endif

//
// errors_throw
//

namespace ircd::fpe
{
	constexpr const size_t &SA_HEAP_MAX {64};
	thread_local allocator::fixed<struct sigaction, SA_HEAP_MAX> sa_heap;

	extern const std::exception_ptr sigfpe_eptr;
	extern "C" void ircd_fpe_handle_sigfpe(int signum, siginfo_t *const si, void *const uctx);
}


decltype(ircd::fpe::sigfpe_eptr)
ircd::fpe::sigfpe_eptr
{
	std::make_exception_ptr(std::domain_error
	{
		"Floating Point Exception"
	})
};

__attribute__((noreturn))
void
ircd::fpe::ircd_fpe_handle_sigfpe(int signum,
                                  siginfo_t *const si,
                                  void *const uctx)
{
	assert(si);
	assert(signum == SIGFPE);
	std::rethrow_exception(sigfpe_eptr); //TODO: still malloc()'s :/
}

//
// errors_throw::errors_throw
//

ircd::fpe::errors_throw::errors_throw()
:their_sa
{
	new (sa_heap().allocate(1)) struct sigaction,
	[](auto *const ptr)
	{
		sa_heap().deallocate(ptr, 1);
	}
}
,their_fenabled
{
	syscall(::fegetexcept)
}
{
	struct sigaction ours {0};
	ours.sa_sigaction = ircd_fpe_handle_sigfpe;
	ours.sa_flags |= SA_SIGINFO;
	ours.sa_flags |= SA_NODEFER; // Required to throw out of the handler

	syscall(std::fegetexceptflag, &their_fe, FE_ALL_EXCEPT);
	syscall(std::feclearexcept, FE_ALL_EXCEPT);
	syscall(::sigaction, SIGFPE, &ours, their_sa.get());
	syscall(::feenableexcept, FE_ALL_EXCEPT);
}

ircd::fpe::errors_throw::~errors_throw()
noexcept
{
	syscall(::fedisableexcept, FE_ALL_EXCEPT);
	syscall(::sigaction, SIGFPE, their_sa.get(), nullptr);
	syscall(::feenableexcept, their_fenabled);
	syscall(std::fesetexceptflag, &their_fe, FE_ALL_EXCEPT);
}

//
// errors_handle
//

ircd::fpe::errors_handle::errors_handle()
{
	syscall(std::fegetexceptflag, &theirs, FE_ALL_EXCEPT);
	clear_pending();
}

ircd::fpe::errors_handle::~errors_handle()
noexcept(false)
{
	const auto pending(this->pending());
	syscall(std::fesetexceptflag, &theirs, FE_ALL_EXCEPT);
	throw_errors(pending);
}

void
ircd::fpe::errors_handle::clear_pending()
{
	syscall(std::feclearexcept, FE_ALL_EXCEPT);
}

void
ircd::fpe::errors_handle::throw_pending()
const
{
	throw_errors(pending());
}

ushort
ircd::fpe::errors_handle::pending()
const
{
	return std::fetestexcept(FE_ALL_EXCEPT);
}

//
// util
//

void
ircd::fpe::throw_errors(const ushort &flags)
{
	if(!flags)
		return;

	thread_local char buf[128];
	throw std::domain_error
	{
		reflect(buf, flags)
	};
}

ircd::string_view
ircd::fpe::reflect(const mutable_buffer &buf,
                   const ushort &flags)
{
	window_buffer wb{buf};
	const auto append{[&wb](const auto &flag)
	{
		wb([&flag](const mutable_buffer &buf)
		{
			return strlcpy(buf, reflect(flag));
		});
	}};

	for(size_t i(0); i < sizeof(flags) * 8; ++i)
		if(flags & (1 << i))
			append(1 << i);

	return wb.completed();
}

ircd::string_view
ircd::fpe::reflect(const ushort &flag)
{
	switch(flag)
	{
		case 0:             return "";
		case FE_INVALID:    return "INVALID";
		case FE_DIVBYZERO:  return "DIVBYZERO";
		case FE_UNDERFLOW:  return "UNDERFLOW";
		case FE_OVERFLOW:   return "OVERFLOW";
		case FE_INEXACT:    return "INEXACT";
	}

	return "?????";
}

ircd::string_view
ircd::fpe::reflect_sicode(const int &code)
{
	switch(code)
	{
		case FPE_INTDIV:    return "INTDIV";
		case FPE_INTOVF:    return "INTOVF";
		case FPE_FLTDIV:    return "FLTDIV";
		case FPE_FLTOVF:    return "FLTOVF";
		case FPE_FLTUND:    return "FLTUND";
		case FPE_FLTRES:    return "FLTRES";
		case FPE_FLTINV:    return "FLTINV";
		case FPE_FLTSUB:    return "FLTSUB";
	}

	return "?????";
}
