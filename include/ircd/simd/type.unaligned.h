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

/// Unaligned type wrapper macro
#define IRCD_SIMD_TYPEDEF_UNALIGNED(TYPE, NAME)  \
struct                                           \
__attribute__((packed))                          \
__attribute__((aligned(1)))                      \
__attribute__((visibility("internal")))          \
NAME                                             \
{                                                \
    TYPE val;                                    \
                                                 \
    operator TYPE () const { return val; }       \
                                                 \
    template<class T> NAME &operator=(T&& t)     \
    {                                            \
        val = std::forward<T>(t);                \
        return *this;                            \
    }                                            \
}

//
// unsigned
//

#ifdef HAVE_X86INTRIN_H
namespace ircd
{
	IRCD_SIMD_TYPEDEF_UNALIGNED(__m512i, u512x1_u);
	IRCD_SIMD_TYPEDEF_UNALIGNED(__m256i, u256x1_u);
	IRCD_SIMD_TYPEDEF_UNALIGNED(__m128i, u128x1_u);
}
#endif

//
// signed
//

#ifdef HAVE_X86INTRIN_H
namespace ircd
{
	IRCD_SIMD_TYPEDEF_UNALIGNED(__m512i, i512x1_u);
	IRCD_SIMD_TYPEDEF_UNALIGNED(__m256i, i256x1_u);
	IRCD_SIMD_TYPEDEF_UNALIGNED(__m128i, i128x1_u);
}
#endif

//
// single precision
//

#ifdef HAVE_X86INTRIN_H
namespace ircd
{
	IRCD_SIMD_TYPEDEF_UNALIGNED(__m512, f512x1_u);
	IRCD_SIMD_TYPEDEF_UNALIGNED(__m256, f256x1_u);
	IRCD_SIMD_TYPEDEF_UNALIGNED(__m128, f128x1_u);
}
#endif

//
// double precision
//

#ifdef HAVE_X86INTRIN_H
namespace ircd
{
	IRCD_SIMD_TYPEDEF_UNALIGNED(__m512d, d512x1_u);
	IRCD_SIMD_TYPEDEF_UNALIGNED(__m256d, d256x1_u);
	IRCD_SIMD_TYPEDEF_UNALIGNED(__m128d, d128x1_u);
}
#endif

#pragma GCC diagnostic pop
