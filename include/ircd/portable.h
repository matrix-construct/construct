// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

//
// Portability Macros & Developer Tools
//

#define HAVE_IRCD_PORTABLE_H

//
// For non-clang
//

#ifndef __has_builtin
	#define __has_builtin(x) 0
#endif

//
// Common branch prediction macros
//

#ifndef likely
	#define likely(x) __builtin_expect(!!(x), 1)
#endif

#ifndef unlikely
	#define unlikely(x) __builtin_expect(!!(x), 0)
#endif

//
// 128 bit integer support
//

#if !defined(HAVE_INT128_T) || !defined(HAVE_UINT128_T)
namespace ircd
{
	#if defined(HAVE___INT128_T) && defined(HAVE__UINT128_T)
		using int128_t = __int128_t;
		using uint128_t = __uint128_t;
	#elif defined(HAVE___INT128)
		using int128_t = signed __int128;
		using uint128_t = unsigned __int128;
	#else
		#error "Missing 128 bit integer types on this platform."
	#endif
}
#endif

//
// Trouble; FreeBSD unsigned long ctype
//

#if defined(__FreeBSD__) && !defined(ulong)
	typedef u_long ulong;
#endif

//
// Trouble; clang w/ our custom assert
//

#if defined(RB_ASSERT) && defined(__clang__) && !defined(assert)
	#ifdef NDEBUG
		#define assert(expr) \
			(static_cast<void>(0))
	#else
		#define assert(expr) \
			(static_cast<bool>(expr)? void(0): __assert_fail(#expr, __FILE__, __LINE__, __FUNCTION__))
	#endif
#endif
