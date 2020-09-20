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
#define HAVE_IRCD_SIMD_POPCNT_H

namespace ircd::simd
{
	template<class T> uint popcnt(const T) noexcept;
}

/// Convenience template. Unfortunately this drops to scalar until specific
/// targets and specializations are created.
template<class T>
inline uint
ircd::simd::popcnt(const T a)
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
