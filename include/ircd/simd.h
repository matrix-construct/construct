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
#define HAVE_IRCD_SIMD_H
#include <RB_INC_X86INTRIN_H

#ifdef HAVE_X86INTRIN_H
	#define IRCD_SIMD
#endif

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

#if defined(IRCD_SIMD)
namespace ircd
{
	typedef __v64qu       u8x64;     //  [
	typedef __v32qu       u8x32;     //  [0|1|2|3|4|5|6|7|8|9|a|b|c|d|e|f|0|1|2|3|4|5|6|7|8|9|a|b|c|d|e|f|
	typedef __v16qu       u8x16;     //  [0|1|2|3|4|5|6|7|8|9|a|b|c|d|e|f|
	typedef __v32hu       u16x32;    //  [
	typedef __v16hu       u16x16;    //  [_0_|_1_|_2_|_3_|_4_|_5_|_6_|_7_|_8_|_9_|_a_|_b_|_c_|_d_|_e_|_f_|
	typedef __v8hu        u16x8;     //  [_0_|_1_|_2_|_3_|_4_|_5_|_6_|_7_|
	typedef __v16su       u32x16;    //  [
	typedef __v8su        u32x8;     //  [__0__|__1__|__2__|__3__|__4__|__5__|__6__|__7__|
	typedef __v4su        u32x4;     //  [__0__|__1__|__2__|__3__|
	typedef __v8du        u64x8;     //  [
	typedef __v4du        u64x4;     //  [____0____|____1____|____2____|____3____|
	typedef __v2du        u64x2;     //  [____0____|____1____|
	typedef __m128i       u128x1;    //  [________0________|
	typedef __m128i_u     u128x1_u;  // unaligned
	typedef __m256i       u256x1;    //  [________________0________________|
	typedef __m256i_u     u256x1_u;  // unaligned
	typedef __m512i       u512x1;    //  [_______________________________0________________________________|
	typedef __m512i_u     u512x1_u;  // unaligned
}
#endif

//
// signed
//

#if defined(IRCD_SIMD)
namespace ircd
{
	typedef __v64qi       i8x64;     //  [
	typedef __v32qi       i8x32;     //  [0|1|2|3|4|5|6|7|8|9|a|b|c|d|e|f|0|1|2|3|4|5|6|7|8|9|a|b|c|d|e|f|
	typedef __v16qi       i8x16;     //  [0|1|2|3|4|5|6|7|8|9|a|b|c|d|e|f|
	typedef __v32hi       i16x32;    //  [
	typedef __v16hi       i16x16;    //  [_0_|_1_|_2_|_3_|_4_|_5_|_6_|_7_|_8_|_9_|_a_|_b_|_c_|_d_|_e_|_f_|
	typedef __v8hi        i16x8;     //  [_0_|_1_|_2_|_3_|_4_|_5_|_6_|_7_|
	typedef __v16si       i32x16;    //  [
	typedef __v8si        i32x8;     //  [__0__|__1__|__2__|__3__|__4__|__5__|__6__|__7__|
	typedef __v4si        i32x4;     //  [__0__|__1__|__2__|__3__|
	typedef __v8di        i64x8;     //  [
	typedef __v4di        i64x4;     //  [____0____|____1____|____2____|____3____|
	typedef __v2di        i64x2;     //  [____0____|____1____|
	typedef __m128i       i128x1;    //  [________0________]
	typedef __m128i_u     i128x1_u;  // unaligned
	typedef __m256i       i256x1;    //  [________________0________________|
	typedef __m256i_u     i256x1_u;  // unaligned
	typedef __m512i       i512x1;    //  [_______________________________0________________________________|
	typedef __m512i_u     i512x1_u;  // unaligned
}
#endif

//
// single precision
//

#if defined(IRCD_SIMD)
namespace ircd
{
	typedef __v16qs       f8x16;     //  [0|1|2|3|4|5|6|7|8|9|a|b|c|d|e|f|
	typedef __v16sf       f32x16;    //  [
	typedef __v8sf        f32x8;     //  [__0__|__1__|__2__|__3__|__4__|__5__|__6__|__7__|
	typedef __v4sf        f32x4;     //  [__0__|__1__|__2__|__3__|
	typedef __m128        f128x1;    //  [____|____0____|____|
	typedef __m128_u      f128x1_u;  // unaligned
	typedef __m256        f256x1;    //  [________________0________________|
	typedef __m256_u      f256x1_u;  // unaligned
	typedef __m512        f512x1;    //  [_______________________________0________________________________|
	typedef __m512_u      f512x1_u;  // unaligned
}
#endif

//
// double precision
//

#if defined(IRCD_SIMD)
namespace ircd
{
	typedef __v8df        f64x8;     //  [
	typedef __v4df        f64x4;     //  [____0____|____1____|____2____|____3____|
	typedef __v2df        f64x2;     //  [____0____|____1____|
	typedef __m128d       d128x1;    //  [________0________]
	typedef __m128d_u     d128x1_u;  // unaligned
	typedef __m256d       d256x1;    //  [________________0________________|
	typedef __m256d_u     d256x1_u;  // unaligned
	typedef __m512d       d512x1;    // [_______________________________0________________________________|
	typedef __m512d_u     d512x1_u;  // unaligned
}
#endif

//
// tools
//

namespace ircd::simd
{
	// other words
	struct pshuf_imm8;

	// lane number convenience constants
	extern const u8x32    u8x32_lane_id;
	extern const u16x16  u16x16_lane_id;
	extern const u8x16    u8x16_lane_id;
	extern const u32x8    u32x8_lane_id;
	extern const u16x8    u16x8_lane_id;
	extern const u64x4    u64x4_lane_id;
	extern const u32x4    u32x4_lane_id;
	extern const u64x2    u64x2_lane_id;
	extern const u256x1  u256x1_lane_id;
	extern const u128x1  u128x1_lane_id;

	// xmmx shifter workarounds
	template<int bits, class T> T shl(const T &a) noexcept;
	template<int bits, class T> T shr(const T &a) noexcept;
	template<int bits> u128x1 shl(const u128x1 &a) noexcept;
	template<int bits> u128x1 shr(const u128x1 &a) noexcept;

	// debug print lanes hex
	template<class V>
	string_view print_lane(const mutable_buffer &buf, const V &) noexcept;

	// debug print register hex
	template<class V>
	string_view print_reg(const mutable_buffer &buf, const V &) noexcept;

	// debug print memory hex
	template<class V>
	string_view print_mem(const mutable_buffer &buf, const V &) noexcept;
}

namespace ircd
{
	using simd::shl;
	using simd::shr;
}

/// Shuffle control structure. This represents an 8-bit immediate operand;
/// note it can only be used if constexprs are allowed in your definition unit.
struct ircd::simd::pshuf_imm8
{
	u8 dst3  : 2;  // set src idx for word 3
	u8 dst2  : 2;  // set src idx for word 2
	u8 dst1  : 2;  // set src idx for word 1
	u8 dst0  : 2;  // set src idx for word 0
};

template<int b>
[[using gnu: always_inline, gnu_inline, artificial]]
extern inline ircd::u128x1
ircd::simd::shr(const u128x1 &a)
noexcept
{
	static_assert
	(
		b % 8 == 0, "xmmx register only shifts right at bytewise resolution."
	);

	return _mm_bsrli_si128(a, b / 8);
}

template<int b>
[[using gnu: always_inline, gnu_inline, artificial]]
extern inline ircd::u128x1
ircd::simd::shl(const u128x1 &a)
noexcept
{
	static_assert
	(
		b % 8 == 0, "xmmx register only shifts right at bytewise resolution."
	);

	return _mm_bslli_si128(a, b / 8);
}
