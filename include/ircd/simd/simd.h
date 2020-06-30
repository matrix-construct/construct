// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_SIMD_H

#include "type.h"
#include "traits.h"
#include "lane_cast.h"
#include "print.h"

namespace ircd::simd
{
	template<class T> T popmask(const T) noexcept;
	template<class T> size_t popcount(const T) noexcept;

	// xmmx shifter workarounds
	template<int bits, class T> T shl(const T &a) noexcept;
	template<int bits, class T> T shr(const T &a) noexcept;
	template<int bits> u128x1 shl(const u128x1 &a) noexcept;
	template<int bits> u128x1 shr(const u128x1 &a) noexcept;
	template<int bits> u256x1 shl(const u256x1 &a) noexcept;
	template<int bits> u256x1 shr(const u256x1 &a) noexcept;
}

namespace ircd
{
	using simd::shl;
	using simd::shr;
	using simd::lane_cast;
}

/// Convenience template. Vector compare instructions yield 0xff on equal;
/// sometimes one might need an actual value of 1 for accumulators or maybe
/// some bool-type reason...
template<class T>
inline size_t
ircd::simd::popcount(const T a)
noexcept
{
	size_t i(0), ret(0);
	while(i < lanes(a))
		ret += __builtin_popcountll(a[i++]);

	return ret;
}

/// Convenience template. Vector compare instructions yield 0xff on equal;
/// sometimes one might need an actual value of 1 for accumulators or maybe
/// some bool-type reason...
template<class T>
inline T
ircd::simd::popmask(const T a)
noexcept
{
	return a & 1;
}

#ifdef HAVE_X86INTRIN_H
template<int b>
[[using gnu: always_inline, gnu_inline, artificial]]
extern inline ircd::u128x1
ircd::simd::shr(const u128x1 &a)
noexcept
{
	static_assert
	(
		b % 8 == 0, "xmmx register only shifts right at bytewise resolution."
	);

	return _mm_bsrli_si128(a, b / 8);
}
#endif

#ifdef HAVE_X86INTRIN_H
template<int b>
[[using gnu: always_inline, gnu_inline, artificial]]
extern inline ircd::u128x1
ircd::simd::shl(const u128x1 &a)
noexcept
{
	static_assert
	(
		b % 8 == 0, "xmmx register only shifts right at bytewise resolution."
	);

	return _mm_bslli_si128(a, b / 8);
}
#endif

#if defined(HAVE_X86INTRIN_H) && defined(__AVX2__)
template<int b>
[[using gnu: always_inline, gnu_inline, artificial]]
extern inline ircd::u256x1
ircd::simd::shr(const u256x1 &a)
noexcept
{
	static_assert
	(
		b % 8 == 0, "ymmx register only shifts right at bytewise resolution."
	);

	return _mm256_srli_si256(a, b / 8);
}
#endif

#if defined(HAVE_X86INTRIN_H) && defined(__AVX2__)
template<int b>
[[using gnu: always_inline, gnu_inline, artificial]]
extern inline ircd::u256x1
ircd::simd::shl(const u256x1 &a)
noexcept
{
	static_assert
	(
		b % 8 == 0, "ymmx register only shifts right at bytewise resolution."
	);

	return _mm256_slli_si256(a, b / 8);
}
#endif
