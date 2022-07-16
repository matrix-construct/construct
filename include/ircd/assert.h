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

// This file has to be included prior to the standard library assert headers
#if defined(RB_ASSERT) && defined(_ASSERT_H_DECLS) && defined(RB_DEBUG)
	#error "Do not include <assert.h> or <cassert> first."
#endif

// Define an indicator for whether we override standard assert() behavior in
// this build if the conditions are appropriate.
#if defined(RB_ASSERT) && !defined(NDEBUG)
	#define _ASSERT_H_DECLS
#endif

// Our utils
namespace ircd
{
	void debugtrap() noexcept;

	template<class expr>
	void always_assert(expr&&) noexcept;

	[[gnu::cold]]
	void print_assertion(const char *, const char *, const unsigned, const char *) noexcept;

	#if defined(RB_ASSERT_OPTIMISTIC)
	struct assertion extern assertion;
	#endif
}

// Alternative declaration of __assert_fail() which may return; our definitions
// take on different behaviors depending on the value of RB_ASSERT.
#if defined(RB_ASSERT)
extern "C" void
__attribute__((visibility("default")))
__assert_fail(const char *__assertion,
              const char *__file,
              unsigned int __line,
              const char *__function) noexcept;
#endif

// Custom assert
#if defined(RB_ASSERT_INTRINSIC) && !defined(NDEBUG)
	#undef assert
	#define assert(expr)                                             \
	({                                                               \
	    if(__builtin_expect(!static_cast<bool>(expr), 0))            \
	        __assert_fail(#expr, __FILE__, __LINE__, __FUNCTION__);  \
	})
#elif defined(RB_ASSERT_OPTIMISTIC) && !defined(NDEBUG)
	#undef assert
	#define assert(expr)                                \
	({                                                  \
	    ircd::assertion.ok &= static_cast<bool>(expr);  \
	})
#endif

// Custom addl cases to avoid any trouble w/ clang
#if defined(RB_ASSERT) && defined(__clang__) && !defined(assert) && defined(NDEBUG)
	#define assert(expr) (static_cast<void>(0))
#endif

#if defined(RB_ASSERT_OPTIMISTIC)
/// State structure for optimistic assertion mode.
struct ircd::assertion
{
	bool ok alignas(8) {true};
	const char *__assertion;
	const char *__file;
	unsigned int __line;
	const char *__function;

	bool point();
};
#endif

#if defined(RB_ASSERT_OPTIMISTIC)
/// Test-and-reset for pending assertion.
[[gnu::hot]]
inline bool
ircd::assertion::point()
{
	if(__builtin_expect(!ok, 0))
	{
		#if defined(__SSE2__) && defined(__clang__)
		asm volatile ("lfence");
		#endif
		__assert_fail(__assertion, __file, __line, __function);
		ok = true;
		return true;
	}
	else return false;
}
#endif

#if defined(RB_ASSERT_INTRINSIC)
extern "C" // for clang
{
	/// Override the standard assert behavior, if enabled, to trap into the
	/// debugger as close as possible to the offending site.
	extern inline void
	__attribute__((cold, flatten, always_inline, gnu_inline, artificial))
	__assert_fail(const char *const __assertion,
	              const char *const __file,
	              unsigned int __line,
	              const char *const __function)
	noexcept
	{
		#if defined(__SSE2__) && defined(__clang__)
		asm volatile ("lfence");
		#endif
		ircd::print_assertion(__assertion, __file, __line, __function);
		ircd::debugtrap();
	}
}
#endif

/// Intrinsic to halt execution for examination by a tracing debugger without
/// aborting the program.
///
extern inline void
__attribute__((always_inline, gnu_inline, artificial))
ircd::debugtrap()
noexcept
{
	#if defined(__clang__)
		static_assert(__has_builtin(__builtin_debugtrap));
		__builtin_debugtrap();
	#elif defined(__x86_64__)
		__asm__ volatile ("int $3   # IRCd ASSERTION DEBUG TRAP !!! ");
	#else
		__builtin_trap();
	#endif
}

/// Trap on false expression whether or not NDEBUG.
template<class expr>
extern inline void
__attribute__((always_inline, gnu_inline, artificial))
ircd::always_assert(expr&& x)
noexcept
{
	if(__builtin_expect(!bool(x), 0))
		debugtrap();
}
