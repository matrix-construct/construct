// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2023 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_UTIL_POPCOUNT_H

namespace ircd {
inline namespace util
{
	template<class T>
	constexpr int popcount(const T) noexcept;

	template<> constexpr int popcount(const unsigned long long) noexcept;
	template<> constexpr int popcount(const unsigned long) noexcept;
	template<> constexpr int popcount(const unsigned int) noexcept;
}}

template<>
constexpr int
ircd::util::popcount(const unsigned long long a)
noexcept
{
	#if __has_feature(__cpp_lib_bitops)
		return std::popcount(a);
	#else
		return __builtin_popcountll(a);
	#endif
}

template<>
constexpr int
ircd::util::popcount(const unsigned long a)
noexcept
{
	#if __has_feature(__cpp_lib_bitops)
		return std::popcount(a);
	#else
		return __builtin_popcountl(a);
	#endif
}

template<>
constexpr int
ircd::util::popcount(const unsigned int a)
noexcept
{
	#if __has_feature(__cpp_lib_bitops)
		return std::popcount(a);
	#else
		return __builtin_popcount(a);
	#endif
}
