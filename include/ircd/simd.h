// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

//
// This header is not included with the standard include group (ircd.h).
// Include this header in specific units as necessary.
//

#pragma once
#define HAVE_IRCD_INTRIN_H
#include <RB_INC_X86INTRIN_H

//
// scalar
//

namespace ircd
{
	typedef int8_t        i8;
	typedef int16_t       i16;
	typedef int32_t       i32;
	typedef int64_t       i64;
	typedef int128_t      i128;
	typedef uint8_t       u8;
	typedef uint16_t      u16;
	typedef uint32_t      u32;
	typedef uint64_t      u64;
	typedef uint128_t     u128;
	typedef float         f32;
	typedef double        f64;
	typedef long double   f128;
}

//
// unsigned
//

#if defined(HAVE_X86INTRIN_H)
namespace ircd
{
	typedef __v32qu       u8x32;     //  [0|1|2|3|4|5|6|7|8|9|a|b|c|d|e|f|0|1|2|3|4|5|6|7|8|9|a|b|c|d|e|f|
	typedef __v16qu       u8x16;     //  [0|1|2|3|4|5|6|7|8|9|a|b|c|d|e|f|
	typedef __v16hu       u16x16;    //  [_0_|_1_|_2_|_3_|_4_|_5_|_6_|_7_|_8_|_9_|_a_|_b_|_c_|_d_|_e_|_f_|
	typedef __v8hu        u16x8;     //  [_0_|_1_|_2_|_3_|_4_|_5_|_6_|_7_|
	typedef __v16su       u32x16;
	typedef __v8su        u32x8;     //  [__0__|__1__|__2__|__3__|__4__|__5__|__6__|__7__|
	typedef __v4su        u32x4;     //  [__0__|__1__|__2__|__3__|
	typedef __v4du        u64x4;     //  [____0____|____1____|____2____|____3____|
	typedef __v2du        u64x2;     //  [____0____|____1____|
	typedef __m128i       u128x1;    //  [________0________|
	typedef __m128i_u     u128x1_u;  //  unaligned
	typedef __m256i       u256x1;    //  [________________0________________|
	typedef __m256i_u     u256x1_u;  //  unaligned
}
#endif

//
// signed
//

#if defined(HAVE_X86INTRIN_H)
namespace ircd
{
	typedef __v32qi       i8x32;     //  [0|1|2|3|4|5|6|7|8|9|a|b|c|d|e|f|0|1|2|3|4|5|6|7|8|9|a|b|c|d|e|f|
	typedef __v16qi       i8x16;     //  [0|1|2|3|4|5|6|7|8|9|a|b|c|d|e|f|
	typedef __v16hi       i16x16;    //  [_0_|_1_|_2_|_3_|_4_|_5_|_6_|_7_|_8_|_9_|_a_|_b_|_c_|_d_|_e_|_f_|
	typedef __v8hi        i16x8;     //  [_0_|_1_|_2_|_3_|_4_|_5_|_6_|_7_|
	typedef __v16si       i32x16;
	typedef __v8si        i32x8;     //  [__0__|__1__|__2__|__3__|__4__|__5__|__6__|__7__|
	typedef __v4si        i32x4;     //  [__0__|__1__|__2__|__3__|
	typedef __v4di        i64x4;     //  [____0____|____1____|____2____|____3____|
	typedef __v2di        i64x2;     //  [____0____|____1____|
	typedef __m128i       i128x1;    //  [________0________]
	typedef __m128i_u     i128x1_u;  //  unaligned
	typedef __m256i       i256x1;    //  [________________0________________|
	typedef __m256i_u     i256x1_u;  //  unaligned
}
#endif

//
// single precision
//

#if defined(HAVE_X86INTRIN_H)
namespace ircd
{
	typedef __v16qs       f8x16;     //  [0|1|2|3|4|5|6|7|8|9|a|b|c|d|e|f|
	typedef __v8sf        f32x8;     //  [__0__|__1__|__2__|__3__|__4__|__5__|__6__|__7__|
	typedef __v4sf        f32x4;     //  [__0__|__1__|__2__|__3__|
	typedef __m128        f128x1;    //  [____|____0____|____|
	typedef __m128_u      f128x1_u;  // unaligned
	typedef __m256        f256x1;    //  [____|____|____|____0____|____|____|____|
	typedef __m256_u      f256x1_u;  // unaligned
}
#endif

//
// double precision
//

#if defined(HAVE_X86INTRIN_H)
namespace ircd
{
	typedef __v4df        f64x4;     //  [____0____|____1____|____2____|____3____|
	typedef __v2df        f64x2;     //  [____0____|____1____|
	typedef __m128d       d128x1;    //  [________0________]
	typedef __m128d_u     d128x1_u;  // unaligned
	typedef __m256d       d256x1;    //  [________|________0________|________|
	typedef __m256d_u     d256x1_u;  // unaligned
}
#endif

//
// other words
//

namespace ircd
{
	struct pshuf_imm8;
}

/// Shuffle control structure. This represents an 8-bit immediate operand;
/// note it can only be used if constexprs are allowed in your definition unit.
struct ircd::pshuf_imm8
{
	u8 dst3  : 2;  // set src idx for word 3
	u8 dst2  : 2;  // set src idx for word 2
	u8 dst1  : 2;  // set src idx for word 1
	u8 dst0  : 2;  // set src idx for word 0
};
