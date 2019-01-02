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
#define HAVE_IRCD_UTIL_H

namespace ircd
{
	/// Utilities for IRCd.
	///
	/// This is an inline namespace: everything declared in it will be
	/// accessible in ircd::. By first opening it here as inline all
	/// subsequent openings of this namespace do not have to use the inline
	/// keyword but will still be inlined to ircd::.
	inline namespace util {}
}

//
// Fundamental macros
//

#define IRCD_EXPCAT(a, b)   a ## b
#define IRCD_CONCAT(a, b)   IRCD_EXPCAT(a, b)
#define IRCD_UNIQUE(a)      IRCD_CONCAT(a, __COUNTER__)

#include "typography.h"
#include "unit_literal.h"
#include "unwind.h"
#include "reentrance.h"
#include "enum.h"
#include "syscall.h"
#include "env.h"
#include "va_rtti.h"
#include "unique_iterator.h"
#include "instance_list.h"
#include "instance_multimap.h"
#include "callbacks.h"
#include "bswap.h"
#include "tuple.h"
#include "timer.h"
#include "scope_light.h"
#include "life_guard.h"
#include "pubsetbuf.h"
#include "string.h"
#include "pointers.h"
#include "iterator.h"
#include "nothrow.h"
#include "what.h"
#include "u2a.h"
#include "pretty.h"
#include "hash.h"

// Unsorted section
namespace ircd {
namespace util {

/// A standard unique_ptr but accepting an std::function for T as its custom
/// deleter. This reduces the boilerplate burden on declaring the right
/// unique_ptr template for custom deleters every single time.
///
template<class T>
using custom_ptr = std::unique_ptr<T, std::function<void (T *)>>;

//
// Misc size() participants.
//

size_t size(std::ostream &s);

template<size_t SIZE>
constexpr size_t
size(const char (&buf)[SIZE])
{
	return SIZE;
}

template<size_t SIZE>
constexpr size_t
size(const std::array<const char, SIZE> &buf)
{
	return SIZE;
}

template<size_t SIZE>
constexpr size_t
size(const std::array<char, SIZE> &buf)
{
	return SIZE;
}

template<class T>
constexpr typename std::enable_if<std::is_integral<T>::value, size_t>::type
size(const T &val)
{
	return sizeof(T);
}

//
// Misc data() participants
//

template<size_t SIZE>
constexpr const char *
data(const char (&buf)[SIZE])
{
	return buf;
}

template<size_t SIZE>
constexpr char *
data(char (&buf)[SIZE])
{
	return buf;
}

template<class T>
constexpr typename std::enable_if<std::is_pod<T>::value, const uint8_t *>::type
data(const T &val)
{
	return reinterpret_cast<const uint8_t *>(&val);
}

template<class T>
constexpr typename std::enable_if<std::is_pod<T>::value, uint8_t *>::type
data(T &val)
{
	return reinterpret_cast<uint8_t *>(&val);
}

//
// Misc bang participants
//

inline auto
operator!(const std::string &str)
{
	return str.empty();
}

inline auto
operator!(const std::string_view &str)
{
	return str.empty();
}

/// Like std::next() but with out_of_range exception
///
template<class It>
typename std::enable_if<is_forward_iterator<It>() || is_input_iterator<It>(), It>::type
at(It &&start,
   It &&stop,
   ssize_t i)
{
	for(; start != stop; --i, std::advance(start, 1))
		if(!i)
			return start;

	throw std::out_of_range
	{
		"at(a, b, i): 'i' out of range"
	};
}

/// Like std::find() -> std::distance(), but with out_of_range exception
///
template<class It,
         class value>
typename std::enable_if<is_forward_iterator<It>() || is_input_iterator<It>(), size_t>::type
index(const It &start,
      const It &stop,
      value &&val)
{
	const auto it
	{
		std::find(start, stop, std::forward<value>(val))
	};

	if(likely(it != stop))
		return std::distance(start, it);

	throw std::out_of_range
	{
		"index(a, b, val): 'val' not contained in set"
	};
}

//
// Some functors for STL
//

template<class container>
struct keys
{
	auto &operator()(typename container::reference v) const
	{
		return v.first;
	}
};

template<class container>
struct values
{
	auto &operator()(typename container::reference v) const
	{
		return v.second;
	}
};

/// Compile-time comparison of string literals
///
constexpr bool
_constexpr_equal(const char *a,
                 const char *b)
{
	return *a == *b && (*a == '\0' || _constexpr_equal(a + 1, b + 1));
}

/// Iterator based until() matching std::for_each except the function
/// returns a bool to continue rather than void.
///
template<class it_a,
         class it_b,
         class boolean_function>
bool
until(it_a a,
      const it_b &b,
      boolean_function&& f)
{
	for(; a != b; ++a)
		if(!f(*a))
			return false;

	return true;
}

constexpr bool
is_powerof2(const long long v)
{
	return v && !(v & (v - 1LL));
}

} // namespace util
} // namespace ircd
