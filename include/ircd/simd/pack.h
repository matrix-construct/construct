// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2021 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_SIMD_PACK_H

// Pack values left overwriting unmasked lanes.
namespace ircd::simd
{
	template<class T,
	         class U>
	T pack(const T vals, const U mask) noexcept;
}

/// Left-pack values to eliminate unmasked lanes.
/// TODO: WIP
template<class T,
         class U>
inline T
ircd::simd::pack(const T val,
                 const U mask)
noexcept
{
	static_assert
	(
		lanes<T>() == lanes<U>()
	);

	static_assert
	(
		std::is_integral<lane_type<U>>()
	);

	U idx{0}, add(mask & 1);
	for(uint i(0); i < lanes<U>(); ++i)
	{
		add = shl<sizeof_lane<U>() * 8L>(add);
		idx += add;
	}

	return shuf(val, idx);
}
