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
#define HAVE_IRCD_SIMD_TRAITS_H

namespace ircd::simd
{
	template<class T>
	static constexpr size_t lanes();

	template<class U,
	         class T>
	static constexpr bool is_lane_same();

	template<class U,
	         class T>
	static constexpr bool is_lane_base_of();

	template<class T>
	static constexpr bool is_lane_integral();

	template<class T>
	static constexpr bool is_lane_floating();

	template<class T>
	T mask_full();

	constexpr auto alignment
	{
		support::avx512f?  64:
		support::avx?      32:
		support::sse?      16:
		                    8
	};
}

template<class T>
[[gnu::const]]
inline T
ircd::simd::mask_full()
{
	if constexpr(is_lane_floating<T>())
		return T{};
	else
		return ~T{0};
}

template<class T>
constexpr bool
ircd::simd::is_lane_floating()
{
	return std::is_floating_point<lane_type<T>>::value;
}

template<class T>
constexpr bool
ircd::simd::is_lane_integral()
{
	return std::is_integral<lane_type<T>>::value;
}

template<class U,
         class T>
constexpr bool
ircd::simd::is_lane_same()
{
	return std::is_same<U, lane_type<T>>::value;
}

template<class U,
         class T>
constexpr bool
ircd::simd::is_lane_base_of()
{
	return std::is_base_of<U, lane_type<T>>::value;
}

/// Get number of lanes for vector type (the number after the x in the
/// type name).
template<class T>
constexpr size_t
ircd::simd::lanes()
{
	constexpr size_t ret
	{
		sizeof(T) / sizeof_lane<T>()
	};

	static_assert
	(
		sizeof(T) <= 64 && ret <= 64 && ret > 0
	);

	return ret;
}

template<>
constexpr size_t
ircd::simd::sizeof_lane<ircd::u128x1>()
{
	return 16;
}

template<>
constexpr size_t
ircd::simd::sizeof_lane<ircd::u256x1>()
{
	return 32;
}

template<>
constexpr size_t
ircd::simd::sizeof_lane<ircd::u512x1>()
{
	return 64;
}
