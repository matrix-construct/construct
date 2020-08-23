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
#define HAVE_IRCD_SIMD_TZCNT_H

namespace ircd::simd
{
	template<class T> uint tzcnt(const T) noexcept;
}

/// Convenience template. Unfortunately this drops to scalar until specific
/// targets and specializations are created. The behavior of can differ among
/// platforms; we use lzcnt when available, otherwise we account for bsr.
template<class T>
inline uint
ircd::simd::tzcnt(const T a)
noexcept
{
	uint ret(0), i(lanes<T>() - 1), mask(-1U); do
	{
		if constexpr(sizeof_lane<T>() <= sizeof(u8))
			ret += mask & __builtin_ctz(a[i] | 0xffffff00U);

		else if constexpr(sizeof_lane<T>() <= sizeof(u16))
			ret += mask & __builtin_ctz(__builtin_bswap16(a[i]) | 0xffff0000U);

		else if constexpr(sizeof_lane<T>() <= sizeof(u32))
			ret += mask &
			(
				(boolmask(uint(a[i] != 0)) & __builtin_ctz(__builtin_bswap32(a[i])))
				| (boolmask(uint(a[i] == 0)) & 32U)
			);

		else if constexpr(sizeof_lane<T>() <= sizeof(u64))
			ret += mask &
			(
				(boolmask(uint(a[i] != 0)) & __builtin_ctzl(__builtin_bswap64(a[i])))
				| (boolmask(uint(a[i] == 0)) & 64U)
			);

		static const auto lane_bits(sizeof_lane<T>() * 8);
		mask &= boolmask(uint(ret % lane_bits == 0));
		mask &= boolmask(uint(ret != 0));
	}
	while(i--);

	return ret;
}
