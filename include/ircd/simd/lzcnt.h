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
/// targets and specializations are created.
template<class T>
inline uint
__attribute__((target("lzcnt")))
ircd::simd::lzcnt(const T a)
noexcept
{
	// The behavior of lzcnt can differ among platforms; when true we expect
	// lzcnt to fall back to bsr-like behavior.
	constexpr auto bitscan
	{
		#ifdef __LZCNT__
			false
		#else
			true
		#endif
	};

	uint ret(0), i(0); do
	{
		const auto mask
		{
			boolmask(uint(ret == sizeof_lane<T>() * 8 * i))
		};

		if constexpr(bitscan && sizeof_lane<T>() <= sizeof(u16))
			ret += (15 - __lzcnt16(__builtin_bswap16(a[i++]))) & mask;

		else if constexpr(sizeof_lane<T>() <= sizeof(u16))
			ret += __lzcnt16(__builtin_bswap16(a[i++])) & mask;

		else if constexpr(bitscan && sizeof_lane<T>() <= sizeof(u32))
			ret += (31 - __lzcnt32(__builtin_bswap32(a[i++]))) & mask;

		else if constexpr(sizeof_lane<T>() <= sizeof(u32))
			ret += __lzcnt32(__builtin_bswap32(a[i++])) & mask;

		else if constexpr(bitscan)
			ret += (63 - __lzcnt64(__builtin_bswap64(a[i++]))) & mask;

		else
			ret += __lzcnt64(__builtin_bswap64(a[i++])) & mask;
	}
	while(i < lanes<T>());

	return ret;
}
