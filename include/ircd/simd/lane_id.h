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
#define HAVE_IRCD_SIMD_LANE_ID_H

namespace ircd::simd
{
	template<class T>
	static constexpr T lane_id();

	// lane number convenience constants
	extern const u8x64    u8x64_lane_id;
	extern const u8x32    u8x32_lane_id;
	extern const u16x32  u16x32_lane_id;
	extern const u8x16    u8x16_lane_id;
	extern const u16x16  u16x16_lane_id;
	extern const u32x16  u32x16_lane_id;
	extern const u16x8    u16x8_lane_id;
	extern const u32x8    u32x8_lane_id;
	extern const u64x8    u64x8_lane_id;
	extern const u32x4    u32x4_lane_id;
	extern const u64x4    u64x4_lane_id;
	extern const u64x2    u64x2_lane_id;
	extern const u128x1  u128x1_lane_id;
	extern const u256x1  u256x1_lane_id;
	extern const u512x1  u512x1_lane_id;
}

template<class T>
constexpr T
ircd::simd::lane_id()
{
	T ret;
	for(uint i(0); i < lanes<T>(); ++i)
		ret[i] = i;

	return ret;
}
