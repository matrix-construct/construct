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

#pragma once
#define HAVE_IRCD_PORTABLE_H

//
// For non-clang
//

#ifndef __has_builtin
	#define __has_builtin(x) 0
#endif

#ifndef __is_identifier
	#define __is_identifier(x) 0
#endif

#ifndef __has_feature
	#define __has_feature(x) 0
#endif

#ifndef __has_cpp_attribute
	#define __has_cpp_attribute(x) 0
#endif

//
// Common branch prediction macros
//

#if !defined(likely)
	#if __has_builtin(__builtin_expect)
		#define likely(x) __builtin_expect(!!(x), 1)
	#else
		#define likely(x) (x)
	#endif
#endif

#if !defined(unlikely)
	#if __has_builtin(__builtin_expect)
		#define unlikely(x) __builtin_expect(!!(x), 0)
	#else
		#define unlikely(x) (x)
	#endif
#endif

#if !defined(unpredictable)
	#if __has_builtin(__builtin_unpredictable)
		#define unpredictable(x) __builtin_unpredictable(!!(x))
	#else
		#define unpredictable(x) (x)
	#endif
#endif

//
// Assume
//

#if !defined(assume)
	#if __has_builtin(__builtin_assume) && defined(__cplusplus)
		// If the expression is 'evaluated' (llvm) (which includes any
		// function call not explicitly annotated pure, even when fully
		// inlined) then the assume is diagnosed by -Wassume and ignored.
		// Declaring the bool mitigates this.
		#define assume(x) assert(x); ({ const bool y(x); __builtin_assume(y); })
	#elif __has_builtin(__builtin_assume)
		#define assume(x) assert(x); __builtin_assume(x)
	#else
		#define assume(x) assert(x)
	#endif
#endif

//
// 128 bit integer support
//

#if defined(__cplusplus) && (!defined(HAVE_INT128_T) || !defined(HAVE_UINT128_T))
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
// Other convenience typedefs
//

#if defined(__cplusplus)
namespace ircd
{
	using longlong = long long;
	using ulonglong = unsigned long long;
}
#endif

//
// OpenCL compat
//

#if !defined(uchar)
	typedef unsigned char uchar;
#endif

//
// Trouble; FreeBSD unsigned long ctype
//

#if defined(__FreeBSD__) && !defined(ulong)
	typedef u_long ulong;
#endif
