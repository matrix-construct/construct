// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_STDINC_H
#pragma GCC visibility push(default)

//
// Standard includes
//
// This header includes almost everything we use out of the standard library.
// This is a pre-compiled header. Project build time is significantly reduced
// by doing things this way and C++ std headers have very little namespace
// pollution and risk of conflicts.
//

// Windows Specific
#ifdef _WIN32
	#define WIN32_LEAN_AND_MEAN 1
	#include <RB_INC_WINDOWS_H
	#include <RB_INC_WINSOCK2_H
	#include <RB_INC_WS2TCPIP_H
	#include <RB_INC_IPHLPAPI_H
#endif

// System/platform preambles
extern "C"
{
	// We have our own assert if possible; some system headers forefully
	// redefine it so we can't include this if so.
	#ifndef assert
		#include <RB_INC_ASSERT_H
	#endif

	#include <RB_INC_UNISTD_H
	#include <RB_INC_SYS_TYPES_H
	#include <RB_INC_SYS_UTSNAME_H
}

// Typography
#include <RB_INC_CSTDARG
#include <RB_INC_CSTDDEF
#include <RB_INC_CSTDINT
#include <RB_INC_CSTDLIB
#include <RB_INC_LIMITS
#include <RB_INC_TYPEINDEX
#include <RB_INC_TYPE_TRAITS
#include <RB_INC_CONCEPTS

// Errors
#include <RB_INC_CERRNO
#include <RB_INC_EXCEPTION
#include <RB_INC_SYSTEM_ERROR

// Dynamic memory
#include <RB_INC_NEW
#include <RB_INC_MEMORY

// Containers
#include <RB_INC_VARIANT
#include <RB_INC_OPTIONAL
#include <RB_INC_TUPLE
#include <RB_INC_ARRAY
#include <RB_INC_BITSET
#include <RB_INC_VECTOR
#include <RB_INC_STACK
#include <RB_INC_FORWARD_LIST
#include <RB_INC_LIST
#include <RB_INC_DEQUE
#include <RB_INC_QUEUE
#include <RB_INC_SET
#include <RB_INC_MAP
#include <RB_INC_UNORDERED_MAP

// Strings
#include <RB_INC_CUCHAR
#include <RB_INC_CSTRING
#include <RB_INC_LOCALE
#include <RB_INC_CODECVT
#include <RB_INC_STRING
#include <RB_INC_STRING_VIEW

// Numerics
#include <RB_INC_CFLOAT
#include <RB_INC_CFENV
#include <RB_INC_CMATH
#include <RB_INC_NUMERIC
#include <RB_INC_RANDOM

// Chronography
#include <RB_INC_CTIME
#include <RB_INC_CHRONO

// Concurrency
#include <RB_INC_ATOMIC
#include <RB_INC_THREAD
#include <RB_INC_MUTEX
#include <RB_INC_SHARED_MUTEX
#include <RB_INC_CONDITION_VARIABLE

// Input/Output
#include <RB_INC_CSTDIO
#include <RB_INC_IOSFWD
#include <RB_INC_SSTREAM
#include <RB_INC_FSTREAM
#include <RB_INC_IOMANIP

// Other standard suites
#include <RB_INC_UTILITY
#include <RB_INC_FUNCTIONAL
#include <RB_INC_ALGORITHM
#include <RB_INC_FILESYSTEM

// Other
#include <RB_INC_X86INTRIN_H

// These are #defined in stdio.h. If the system includes it indirectly we have
// to undef those here or there will be trouble.
#undef stdin
#undef stdout
#undef stderr

// Historical macros from types.h
#undef major
#undef minor

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

#pragma GCC visibility pop
