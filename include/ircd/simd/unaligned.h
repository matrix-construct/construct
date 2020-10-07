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
#define HAVE_IRCD_SIMD_TYPE_UNALIGNED_H

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpsabi"
#pragma GCC diagnostic ignored "-Wpacked"

namespace ircd::simd
{
	template<class T> struct unaligned;
}

/// Unaligned wrapper template class.
/// T = inner aligned type
template<class T>
struct
__attribute__((packed))
__attribute__((aligned(1)))
__attribute__((visibility("internal")))
ircd::simd::unaligned
{
	 using value_type = T;

	T val;

	operator T() const
	{
		return val;
	}

	template<class U>
	unaligned(U&& val)
	:val(std::forward<U>(val))
	{}
};

/// Unaligned type wrapper macro. We use this macro to define several common
/// instantiations of unaligned<T>.
///
#define IRCD_SIMD_TYPEDEF_UNALIGNED(TYPE, NAME)   \
namespace ircd                                    \
{                                                 \
    namespace simd                                \
    {                                             \
        using NAME = unaligned<TYPE>;             \
    }                                             \
                                                  \
    using simd::NAME;                             \
}

//
// unsigned
//

IRCD_SIMD_TYPEDEF_UNALIGNED(m512i, u512x1_u);
IRCD_SIMD_TYPEDEF_UNALIGNED(m256i, u256x1_u);
IRCD_SIMD_TYPEDEF_UNALIGNED(m128i, u128x1_u);

//
// signed
//

IRCD_SIMD_TYPEDEF_UNALIGNED(m512i, i512x1_u);
IRCD_SIMD_TYPEDEF_UNALIGNED(m256i, i256x1_u);
IRCD_SIMD_TYPEDEF_UNALIGNED(m128i, i128x1_u);

//
// single precision
//

IRCD_SIMD_TYPEDEF_UNALIGNED(m512f, f512x1_u);
IRCD_SIMD_TYPEDEF_UNALIGNED(m256f, f256x1_u);
IRCD_SIMD_TYPEDEF_UNALIGNED(m128f, f128x1_u);

//
// double precision
//

IRCD_SIMD_TYPEDEF_UNALIGNED(m512d, d512x1_u);
IRCD_SIMD_TYPEDEF_UNALIGNED(m256d, d256x1_u);
IRCD_SIMD_TYPEDEF_UNALIGNED(m128d, d128x1_u);

#pragma GCC diagnostic pop
