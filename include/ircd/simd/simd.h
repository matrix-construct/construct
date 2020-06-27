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
#include "print.h"

namespace ircd::simd
{
	// xmmx shifter workarounds
	template<int bits, class T> T shl(const T &a) noexcept;
	template<int bits, class T> T shr(const T &a) noexcept;
	template<int bits> u128x1 shl(const u128x1 &a) noexcept;
	template<int bits> u128x1 shr(const u128x1 &a) noexcept;
	template<int bits> u256x1 shl(const u256x1 &a) noexcept;
	template<int bits> u256x1 shr(const u256x1 &a) noexcept;

	template<class R, class T> R lane_cast(const T &);
}

namespace ircd
{
	using simd::shl;
	using simd::shr;
}

/// Convert each lane from a smaller type to a larger type
template<class R,
         class T>
inline R
ircd::simd::lane_cast(const T &in)
{
	static_assert
	(
		lanes<R>() == lanes<T>(), "Types must have matching number of lanes."
	);

	if constexpr(__has_builtin(__builtin_convertvector))
		return __builtin_convertvector(in, R);

	R ret;
	for(size_t i(0); i < lanes<R>(); ++i)
		ret[i] = in[i];

	return ret;
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
