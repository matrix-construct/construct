// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#define HAVE_IRCD_STDINC_H

///////////////////////////////////////////////////////////////////////////////
//
// Standard includes
//
// This header includes almost everything we use out of the standard library.
// This is a pre-compiled header. Project build time is significantly reduced
// by doing things this way and C++ std headers have very little namespace
// pollution and risk of conflicts.
//

// It is advised that all standard library symbols are declared with default
// visibility. This pragma overrides any -fvisibility option on any unit.
#pragma GCC visibility push(default)

extern "C"
{
	#include <RB_INC_ASSERT_H
	#include <RB_INC_STDARG_H
	#include <RB_INC_UNISTD_H
	#include <RB_INC_SYS_TYPES_H
	#include <RB_INC_SYS_TIME_H
	#include <RB_INC_SYS_UTSNAME_H
}

#ifdef _WIN32
	#define WIN32_LEAN_AND_MEAN 1
	#include <RB_INC_WINDOWS_H
	#include <RB_INC_WINSOCK2_H
	#include <RB_INC_WS2TCPIP_H
	#include <RB_INC_IPHLPAPI_H
#endif

#include <RB_INC_CSTDDEF
#include <RB_INC_CSTDINT
#include <RB_INC_CSTDLIB
#include <RB_INC_LIMITS
#include <RB_INC_TYPE_TRAITS
#include <RB_INC_TYPEINDEX
#include <RB_INC_VARIANT
#include <RB_INC_CERRNO
#include <RB_INC_UTILITY
#include <RB_INC_FUNCTIONAL
#include <RB_INC_ALGORITHM
#include <RB_INC_NUMERIC
#include <RB_INC_CMATH
#include <RB_INC_CFENV
#include <RB_INC_MEMORY
#include <RB_INC_EXCEPTION
#include <RB_INC_SYSTEM_ERROR
#include <RB_INC_ARRAY
#include <RB_INC_VECTOR
#include <RB_INC_STACK
#include <RB_INC_STRING
#include <RB_INC_CSTRING
#include <RB_INC_STRING_VIEW
#include <RB_INC_LOCALE
#include <RB_INC_CODECVT
#include <RB_INC_MAP
#include <RB_INC_SET
#include <RB_INC_LIST
#include <RB_INC_FORWARD_LIST
#include <RB_INC_UNORDERED_MAP
#include <RB_INC_DEQUE
#include <RB_INC_QUEUE
#include <RB_INC_SSTREAM
#include <RB_INC_FSTREAM
#include <RB_INC_IOSFWD
#include <RB_INC_IOMANIP
#include <RB_INC_CSTDIO
#include <RB_INC_CHRONO
#include <RB_INC_CTIME
#include <RB_INC_ATOMIC
#include <RB_INC_THREAD
#include <RB_INC_MUTEX
#include <RB_INC_SHARED_MUTEX
#include <RB_INC_CONDITION_VARIABLE
#include <RB_INC_RANDOM
#include <RB_INC_BITSET
#include <RB_INC_OPTIONAL
#include <RB_INC_NEW
#include <RB_INC_FILESYSTEM

#include <RB_INC_EXPERIMENTAL_STRING_VIEW
#include <RB_INC_EXPERIMENTAL_OPTIONAL

// FreeBSD unsigned long ctype
#if defined(__FreeBSD__) && !defined(ulong)
	typedef u_long ulong;
#endif

// These are #defined in stdio.h. If the system includes it indirectly we have
// to undef those here or there will be trouble.
#undef stdin
#undef stdout
#undef stderr

// Trouble. clang++-8
#ifndef assert
	#define assert(expr) (static_cast<void>(0))
#endif

//////////////////////////////////////////////////////////////////////////////>
//
// Pollution
//
// This section lists all of the items introduced outside of our namespace
// which may conflict with your project.
//

// Common branch prediction macros
#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)

// Experimental std::string_view
#if !defined(__cpp_lib_string_view) && defined(__cpp_lib_experimental_string_view)
namespace std
{
	using experimental::string_view;
}
#endif

// Experimental std::optional
#if !defined(__cpp_lib_optional) && defined(__cpp_lib_experimental_optional)
namespace std
{
	using experimental::optional;
}
#endif

// Forward declare the existence of these in std:: to allow their immediate
// use throughout the project as a developer convenience. <iostream> is not
// included here because it generates naive initialization code in every unit,
// whereas we conduct it once for libircd in the right place.
namespace std
{
	extern istream cin;
	extern ostream cout;
	extern ostream cerr;
}

// OpenSSL
// Additional forward declarations in the extern namespace are introduced
// by ircd/openssl.h

///////////////////////////////////////////////////////////////////////////////
//
// libircd API
//
// Some items imported into our namespace.
//

// 128 bit integer support
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

namespace ircd
{
	using std::get;
	using std::end;
	using std::begin;

	using std::nullptr_t;
	using std::nothrow_t;

	using std::const_pointer_cast;
	using std::static_pointer_cast;
	using std::dynamic_pointer_cast;

	using std::chrono::hours;
	using std::chrono::minutes;
	using std::chrono::seconds;
	using std::chrono::milliseconds;
	using std::chrono::microseconds;
	using std::chrono::nanoseconds;
	using std::chrono::duration;
	using std::chrono::duration_cast;
	using std::chrono::system_clock;
	using std::chrono::steady_clock;
	using std::chrono::high_resolution_clock;
	using std::chrono::time_point;

	using namespace std::string_literals;
	using namespace std::chrono_literals;
	using namespace std::literals::chrono_literals;
	using std::string_literals::operator""s;
	using std::chrono_literals::operator""s;

	namespace ph = std::placeholders;

	template<class... T> using ilist = std::initializer_list<T...>;

	using std::error_code;
}

#pragma GCC visibility pop // default
