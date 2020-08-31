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
#define HAVE_IRCD_SIMD_SUM_H

namespace ircd::simd
{
	template<int idx = 0,
	         class T>
	T sum_or(T) noexcept;

	template<int idx = 0,
	         class T>
	T sum_and(T) noexcept;

	template<int idx = 0,
	         class T>
	T sum_xor(T) noexcept;
}

template<int idx,
         class T>
[[using gnu: always_inline, gnu_inline, artificial]]
extern inline T
ircd::simd::sum_and(T a)
noexcept
{
	for(size_t i(0); i < lanes<T>(); ++i)
		if(i != idx)
			a[idx] &= a[i];

	return a;
}

template<int idx,
         class T>
[[using gnu: always_inline, gnu_inline, artificial]]
extern inline T
ircd::simd::sum_or(T a)
noexcept
{
	for(size_t i(0); i < lanes<T>(); ++i)
		if(i != idx)
			a[idx] |= a[i];

	return a;
}

template<int idx,
         class T>
[[using gnu: always_inline, gnu_inline, artificial]]
extern inline T
ircd::simd::sum_xor(T a)
noexcept
{
	for(size_t i(0); i < lanes<T>(); ++i)
		if(i != idx)
			a[idx] ^= a[i];

	return a;
}
