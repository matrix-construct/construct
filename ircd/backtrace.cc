// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <RB_INC_EXECINFO_H
#include <RB_INC_SYS_MMAN_H

#if defined(__GNUC__) && defined(HAVE_EXECINFO_H)
	#define IRCD_BACKTRACE_SUPPORT
#endif

#if defined(IRCD_BACKTRACE_SUPPORT) && defined(HAVE_SYS_MMAN_H) && !defined(__clang__)
	#define IRCD_BACKTRACE_GLIBC_WORKAROUND
#endif

#if defined(IRCD_BACKTRACE_GLIBC_WORKAROUND)
// The problem here is that backtrace(3) performs checks against this value
// which is not correct for our own allocated `ircd::ctx` stacks. To workaround
// this we neutralize the value before calling backtrace(3) and restore it
// afterward. While this symbol is extern accessible, the page its on is
// write-protected at some point, so we have to re-allow writing to it here.
//
// sysdeps/i386/backtrace.c | elf/dl-support.c
// > This is a global variable set at program start time.  It marks the
// > highest used stack address.
//
extern void *
__libc_stack_end;

void
__attribute__((constructor))
ircd_backtrace_allow_libc_fix()
{
	static const auto &prot
	{
		PROT_READ | PROT_WRITE
	};

	const auto &addr
	{
		uintptr_t(&__libc_stack_end) & ~(getpagesize() - 1UL)
	};

	ircd::syscall(::mprotect, (void *)addr, sizeof(__libc_stack_end), prot);
}
#endif defined(IRCD_BACKTRACE_GLIBC_WORKAROUND)

//
// backtrace::backtrace
//

namespace ircd
{
	thread_local std::array<const void *, 512> backtrace_buffer;
}

ircd::backtrace::backtrace()
:backtrace
{
	backtrace_buffer.data(),
	backtrace_buffer.size()
}
{
}

ircd::backtrace::backtrace(const mutable_buffer &buf)
:backtrace
{
	reinterpret_cast<const void **>
	(
		const_cast<const char **>
		(
			std::addressof(data(buf))
		)
	),
	buffer::size(buf) / sizeof(void *)
}
{
}

ircd::backtrace::backtrace(const void **const &array,
                           const size_t &count)
:array
{
	array
}
,count
{
	0UL
}
{
	#if defined(IRCD_BACKTRACE_GLIBC_WORKAROUND)
	const scope_restore stack_check_workaround
	{
		__libc_stack_end, reinterpret_cast<void *>(uintptr_t(-1UL))
	};
	#endif

	#if defined(IRCD_BACKTRACE_SUPPORT)
	this->count = ::backtrace(const_cast<void **>(this->array), count);
	#endif
}
