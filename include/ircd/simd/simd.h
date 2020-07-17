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
#include "broad_cast.h"
#include "shift.h"
#include "print.h"

namespace ircd::simd
{
	template<class T> T popmask(const T) noexcept;
	template<class T> T boolmask(const T) noexcept;

	template<class T> uint popcount(const T) noexcept;
	template<class T> uint clz(const T) noexcept;
	template<class T> uint ctz(const T) noexcept;
}

namespace ircd
{
	using simd::lane_cast;
	using simd::shl;
	using simd::shr;
}

/// Convenience template. Unfortunately this drops to scalar until specific
/// targets and specializations are created.
template<class T>
inline uint
__attribute__((target("lzcnt")))
ircd::simd::clz(const T a)
noexcept
{
	constexpr auto uses_bsr
	{
		#ifndef __LZCNT__
			true
		#else
			false
		#endif
	};

	uint ret(0), i(0); do
	{
		const auto mask
		{
			boolmask(uint(ret == sizeof_lane<T>() * 8 * i))
		};

		if constexpr(sizeof_lane<T>() <= sizeof(u16) && uses_bsr)
			ret += (15 - __lzcnt16(__builtin_bswap16(a[i++]))) & mask;
		else if constexpr(sizeof_lane<T>() <= sizeof(u16))
			ret += __lzcnt16(__builtin_bswap16(a[i++])) & mask;
		else if constexpr(sizeof_lane<T>() <= sizeof(u32) && uses_bsr)
			ret += (31 - __lzcnt32(__builtin_bswap32(a[i++]))) & mask;
		else if constexpr(sizeof_lane<T>() <= sizeof(u32))
			ret += __lzcnt32(__builtin_bswap32(a[i++])) & mask;
		else if constexpr(uses_bsr)
			ret += (63 - __lzcnt64(__builtin_bswap64(a[i++]))) & mask;
		else
			ret += __lzcnt64(__builtin_bswap64(a[i++])) & mask;
	}
	while(i < lanes<T>());

	return ret;
}

/// Convenience template. Unfortunately this drops to scalar until specific
/// targets and specializations are created.
template<class T>
inline uint
__attribute__((target("lzcnt")))
ircd::simd::ctz(const T a)
noexcept
{
	constexpr auto uses_bsr
	{
		#ifndef __LZCNT__
			true
		#else
			false
		#endif
	};

	uint ret(0), i(lanes<T>()), mask(-1U); do
	{
		if constexpr(sizeof_lane<T>() <= sizeof(u16) && uses_bsr)
			ret += (15 - __lzcnt16(a[--i])) & mask;
		else if constexpr(sizeof_lane<T>() <= sizeof(u16))
			ret += __lzcnt16(a[--i]) & mask;
		else if constexpr(sizeof_lane<T>() <= sizeof(u32) && uses_bsr)
			ret += (31 - __lzcnt32(a[--i])) & mask;
		else if constexpr(sizeof_lane<T>() <= sizeof(u32))
			ret += __lzcnt32(a[--i]) & mask;
		else if constexpr(uses_bsr)
			ret += (63 - __lzcnt64(a[--i])) & mask;
		else
			ret += __lzcnt64(a[--i]) & mask;

		static const auto lane_bits(sizeof_lane<T>() * 8);
		mask &= boolmask(uint(ret % lane_bits == 0));
		mask &= boolmask(uint(ret != 0));
	}
	while(i);

	return ret;
}

/// Convenience template. Unfortunately this drops to scalar until specific
/// targets and specializations are created.
template<class T>
inline uint
ircd::simd::popcount(const T a)
noexcept
{
	uint ret(0), i(0);
	for(; i < lanes<T>(); ++i)
		if constexpr(sizeof_lane<T>() <= sizeof(int))
			ret += __builtin_popcount(a[i]);
		else if constexpr(sizeof_lane<T>() <= sizeof(long))
			ret += __builtin_popcountl(a[i]);
		else
			ret += __builtin_popcountll(a[i]);

	return ret;
}

/// Convenience template. Extends a bool value where the lsb is 1 or 0 into a
/// mask value like the result of vector comparisons.
template<class T>
inline T
ircd::simd::boolmask(const T a)
noexcept
{
	return ~(popmask(a) - 1);
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
