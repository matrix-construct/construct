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
#define HAVE_IRCD_SIMD_LZCNT_H

namespace ircd::simd
{
	template<class T> uint lzcnt(const T) noexcept;
}

/// Convenience template. Unfortunately this drops to scalar until specific
/// targets and specializations are created. The behavior can differ among
/// platforms; we make use of lzcnt if available otherwise we account for bsr.
template<class T>
inline uint
ircd::simd::lzcnt(const T a)
noexcept
{
	uint ret(0);
	for(uint i(0); i < lanes<T>(); ++i)
	{
		const auto mask
		{
			boolmask(uint(ret == sizeof_lane<T>() * 8 * i))
		};

		if constexpr(sizeof_lane<T>() <= sizeof(u8))
			ret += mask & __builtin_clz((uint(a[i]) << 24) | 0x00ffffff);

		else if constexpr(sizeof_lane<T>() <= sizeof(u16))
			ret += mask & __builtin_clz((uint(__builtin_bswap16(a[i])) << 16) | 0x0000ffffU);

		else if constexpr(sizeof_lane<T>() <= sizeof(u32))
			ret += mask &
			(
				(boolmask(uint(a[i] != 0)) & __builtin_clz(__builtin_bswap32(a[i])))
				| (boolmask(uint(a[i] == 0)) & 32U)
			);

		else if constexpr(sizeof_lane<T>() <= sizeof(u64))
			ret += mask &
			(
				(boolmask(uint(a[i] != 0)) & __builtin_clzl(__builtin_bswap64(a[i])))
				| (boolmask(uint(a[i] == 0)) & 64U)
			);
	}

	return ret;
}
