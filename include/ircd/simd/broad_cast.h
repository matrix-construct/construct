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
#define HAVE_IRCD_SIMD_BROAD_CAST_H

namespace ircd::simd
{
	template<class T,
	         class V>
	T broad_cast(T, const V) noexcept;

	template<class T,
	         class V>
	T broad_cast(const V) noexcept;
}

template<class T,
         class V>
[[using gnu: always_inline, gnu_inline, artificial]]
extern inline T
ircd::simd::broad_cast(const V v)
noexcept
{
	return broad_cast(T{}, v);
}

template<class T,
         class V>
[[using gnu: always_inline, gnu_inline, artificial]]
extern inline T
ircd::simd::broad_cast(T a,
                       const V v)
noexcept
{
	for(size_t i(0); i < lanes<T>(); ++i)
		a[i] = v;

	return a;
}
