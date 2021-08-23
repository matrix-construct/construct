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
#define HAVE_IRCD_SIMD_SHUF_H

namespace ircd::simd
{
	template<class T,
	         class U>
	T
	shuf(const T in,
	     const U dst = lane_id<U>(),
	     const U src = lane_id<U>()) noexcept;
}

/// Move values between lanes in a vector. The destination lane number and
/// source lane number are indexed by respective argument. Each argument is
/// a vector with the same number of lanes as the input value vector. By
/// default each argument vector is lane_id(), which means no-op.
///
template<class T,
         class U>
inline T
ircd::simd::shuf(const T in,
                 const U dst,
                 const U src)
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

	lane_type<T> out alignas(alignof(T)) [lanes<T>()];

	for(uint i(0); i < lanes<T>(); ++i)
		out[dst[i]] = in[src[i]];

	return *reinterpret_cast<const T *>(out);
}
