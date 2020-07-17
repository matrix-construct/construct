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
/// targets and specializations are created.
template<class T>
inline uint
__attribute__((target("lzcnt")))
ircd::simd::tzcnt(const T a)
noexcept
{
    // The behavior of lzcnt/tzcnt can differ among platforms; when false we
    // lzcnt/tzcnt to fall back to bsr/bsf-like behavior.
	constexpr auto bitscan
	{
		#ifdef __LZCNT__
			false
		#else
			true
		#endif
	};

	uint ret(0), i(lanes<T>()), mask(-1U); do
	{
		if constexpr(bitscan && sizeof_lane<T>() <= sizeof(u16))
			ret += (15 - __lzcnt16(a[--i])) & mask;

		else if constexpr(sizeof_lane<T>() <= sizeof(u16))
			ret += __lzcnt16(a[--i]) & mask;

		else if constexpr(bitscan && sizeof_lane<T>() <= sizeof(u32))
			ret += (31 - __lzcnt32(a[--i])) & mask;

		else if constexpr(sizeof_lane<T>() <= sizeof(u32))
			ret += __lzcnt32(a[--i]) & mask;

		else if constexpr(bitscan)
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
