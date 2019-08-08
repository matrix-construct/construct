// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

// This header allows for the use of soft-assertions. It is included before
// the standard <assert.h> and these declarations will take precedence.
//
// These declarations exist because the system declarations may have
// __attribute__((noreturn)) and our only modification here is to remove
// that. Alternative definitions to the standard library also exist in
// ircd/assert.cc

#pragma once
#define HAVE_IRCD_ASSERT_H

namespace ircd
{
	void debugtrap();
}

#if defined(RB_ASSERT) && defined(_ASSERT_H_DECLS) && defined(RB_DEBUG)
	#error "Do not include <assert.h> or <cassert> first."
#endif

#if defined(RB_ASSERT) && !defined(NDEBUG)
#define _ASSERT_H_DECLS

extern "C" void
__attribute__((visibility("default")))
__assert_fail(const char *__assertion,
              const char *__file,
              unsigned int __line,
              const char *__function);

extern "C" void
__attribute__((visibility("default")))
__assert_perror_fail(int __errnum,
                     const char *__file,
                     unsigned int __line,
                     const char *__function);

extern "C" void
__attribute__((visibility("default")))
__assert(const char *__assertion,
         const char *__file,
         int __line);

#endif

extern inline void
__attribute__((always_inline, gnu_inline, artificial))
ircd::debugtrap()
{
	#if defined(__clang__)
		static_assert(__has_builtin(__builtin_debugtrap));
		__builtin_debugtrap();
	#elif defined(__x86_64__)
		__asm__ volatile ("int $3");
	#else
		__builtin_trap();
	#endif
}
