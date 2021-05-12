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
#define HAVE_IRCD_SIMD_TYPE_H

namespace ircd::simd
{
	template<class T>
	using lane_type = typename std::remove_reference<decltype(T{}[0])>::type;

	template<class V,
	         class U = lane_type<V>>
	static constexpr size_t sizeof_lane() = delete;

	template<class T>
	static constexpr bool is()
	{
		return false;
	}
}

#define IRCD_SIMD_TYPEVEC(_T_, _U_, _V_)                      \
namespace ircd                                                \
{                                                             \
    namespace simd                                            \
    {                                                         \
        using _T_ = _U_                                       \
        __attribute__((vector_size((_V_))));                  \
    }                                                         \
                                                              \
    using simd::_T_;                                          \
}

#define IRCD_SIMD_TYPEUSE(_T_, _U_, _V_)                      \
namespace ircd                                                \
{                                                             \
    namespace simd                                            \
    {                                                         \
        using _T_ = _U_                                       \
        __attribute__((aligned((_V_))));                      \
                                                              \
        template<>                                            \
        constexpr size_t sizeof_lane<_T_, _U_>()              \
        {                                                     \
            return sizeof(_U_);                               \
        }                                                     \
    }                                                         \
                                                              \
    using simd::_T_;                                          \
}

#define IRCD_SIMD_TYPEDEF(_T_, _U_, _V_)                      \
namespace ircd                                                \
{                                                             \
    namespace simd                                            \
    {                                                         \
        using _T_ = _U_                                       \
        __attribute__((vector_size((_V_)), aligned((_V_))));  \
                                                              \
        template<>                                            \
        constexpr size_t sizeof_lane<_T_, _U_>()              \
        {                                                     \
            return sizeof(_U_);                               \
        }                                                     \
                                                              \
        template<>                                            \
        constexpr bool is<_T_>()                              \
        {                                                     \
            return true;                                      \
        }                                                     \
    }                                                         \
                                                              \
    using simd::_T_;                                          \
}

//
// scalar
//

namespace ircd
{
	#ifdef HAVE_CHAR8_T
	typedef char8_t       c8;
	#endif
	typedef char16_t      c16;
	typedef char32_t      c32;
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
	#if defined(HAVE___FP8)
	#define HAVE_FP8
	typedef __fp8         f8;
	#endif
	#if defined(HAVE__FLOAT16)
	#define HAVE_FP16
	typedef _Float16      f16;
	#elif defined(HAVE___FP16)
	#define HAVE_FP16
	typedef __fp16        f16;
	#elif defined(__clang__)
	#warning "Missing half-precision floating point support."
	#endif
	typedef float         f32;
	typedef double        f64;
	typedef long double   f128;
}

//
// vector
//

IRCD_SIMD_TYPEVEC(m512f, float,              64)
IRCD_SIMD_TYPEVEC(m512d, double,             64)
IRCD_SIMD_TYPEVEC(m512i, long long,          64)
IRCD_SIMD_TYPEVEC(m512u, unsigned long long, 64)

IRCD_SIMD_TYPEVEC(m256f, float,              32)
IRCD_SIMD_TYPEVEC(m256d, double,             32)
IRCD_SIMD_TYPEVEC(m256i, long long,          32)
IRCD_SIMD_TYPEVEC(m256u, unsigned long long, 32)

IRCD_SIMD_TYPEVEC(m128f, float,              16)
IRCD_SIMD_TYPEVEC(m128d, double,             16)
IRCD_SIMD_TYPEVEC(m128i, long long,          16)
IRCD_SIMD_TYPEVEC(m128u, unsigned long long, 16)

//
// unsigned
//

IRCD_SIMD_TYPEUSE(u512x1, m512u, 64)  // [_______________________________0________________...
IRCD_SIMD_TYPEUSE(u256x1, m256u, 32)  // [________________0________________|
IRCD_SIMD_TYPEUSE(u128x1, m128u, 16)  // [________0________|

IRCD_SIMD_TYPEDEF(u64x8,    u64, 64)  // [____0____|____1____|____2____|____3____|____4___...
IRCD_SIMD_TYPEDEF(u64x4,    u64, 32)  // [____0____|____1____|____2____|...
IRCD_SIMD_TYPEDEF(u64x2,    u64, 16)  // [____0____|____1____]

IRCD_SIMD_TYPEDEF(u32x16,   u32, 64)  // [__0__|__1__|__2__|__3__|__4__|__5__|__6__|__7__|...
IRCD_SIMD_TYPEDEF(u32x8,    u32, 32)  // [__0__|__1__|__2__|__3__|__4__|...
IRCD_SIMD_TYPEDEF(u32x4,    u32, 16)  // [__0__|__1__|__2__|__3__]

IRCD_SIMD_TYPEDEF(u16x32,   u16, 64)  // [_0_|_1_|_2_|_3_|_4_|_5_|_6_|_7_|_8_|_9_|_a_|_b_|...
IRCD_SIMD_TYPEDEF(u16x16,   u16, 32)  // [_0_|_1_|_2_|_3_|_4_|_5_|_6_|_7_|_8_|_9_|...
IRCD_SIMD_TYPEDEF(u16x8,    u16, 16)  // [_0_|_1_|_2_|_3_|_4_|_5_|_6_|_7_]

IRCD_SIMD_TYPEDEF(u8x64,    u8,  64)  // [0|1|2|3|4|5|6|7|8|9|a|b|c|d|e|f|0|1|2|3|4|5|6|7|...
IRCD_SIMD_TYPEDEF(u8x32,    u8,  32)  // [0|1|2|3|4|5|6|7|8|9|a|b|c|d|e|f|0|1|2|3|4...
IRCD_SIMD_TYPEDEF(u8x16,    u8,  16)  // [0|1|2|3|4|5|6|7|8|9|a|b|c|d|e|f]

//
// signed
//

IRCD_SIMD_TYPEUSE(i512x1, m512i, 64)  // [_______________________________0________________...
IRCD_SIMD_TYPEUSE(i256x1, m256i, 32)  // [________________0________________|
IRCD_SIMD_TYPEUSE(i128x1, m128i, 16)  // [________0________|

IRCD_SIMD_TYPEDEF(i64x8,    i64, 64)  // [____0____|____1____|____2____|____3____|____4___...
IRCD_SIMD_TYPEDEF(i64x4,    i64, 32)  // [____0____|____1____|____2____|...
IRCD_SIMD_TYPEDEF(i64x2,    i64, 16)  // [____0____|____1____]

IRCD_SIMD_TYPEDEF(i32x16,   i32, 64)  // [__0__|__1__|__2__|__3__|__4__|__5__|__6__|__7__|...
IRCD_SIMD_TYPEDEF(i32x8,    i32, 32)  // [__0__|__1__|__2__|__3__|__4__|...
IRCD_SIMD_TYPEDEF(i32x4,    i32, 16)  // [__0__|__1__|__2__|__3__]

IRCD_SIMD_TYPEDEF(i16x32,   i16, 64)  // [_0_|_1_|_2_|_3_|_4_|_5_|_6_|_7_|_8_|_9_|_a_|_b_|...
IRCD_SIMD_TYPEDEF(i16x16,   i16, 32)  // [_0_|_1_|_2_|_3_|_4_|_5_|_6_|_7_|_8_|_9_|...
IRCD_SIMD_TYPEDEF(i16x8,    i16, 16)  // [_0_|_1_|_2_|_3_|_4_|_5_|_6_|_7_]

IRCD_SIMD_TYPEDEF(i8x64,    i8,  64)  // [0|1|2|3|4|5|6|7|8|9|a|b|c|d|e|f|0|1|2|3|4|5|6|7|...
IRCD_SIMD_TYPEDEF(i8x32,    i8,  32)  // [0|1|2|3|4|5|6|7|8|9|a|b|c|d|e|f|0|1|2|3|4...
IRCD_SIMD_TYPEDEF(i8x16,    i8,  16)  // [0|1|2|3|4|5|6|7|8|9|a|b|c|d|e|f]

//
// character
//

IRCD_SIMD_TYPEDEF(c32x16,   c32, 64)  // [__0__|__1__|__2__|__3__|__4__|__5__|__6__|__7__|...
IRCD_SIMD_TYPEDEF(c32x8,    c32, 32)  // [__0__|__1__|__2__|__3__|__4__|...
IRCD_SIMD_TYPEDEF(c32x4,    c32, 16)  // [__0__|__1__|__2__|__3__]

IRCD_SIMD_TYPEDEF(c16x32,   c16, 64)  // [_0_|_1_|_2_|_3_|_4_|_5_|_6_|_7_|_8_|_9_|_a_|_b_|...
IRCD_SIMD_TYPEDEF(c16x16,   c16, 32)  // [_0_|_1_|_2_|_3_|_4_|_5_|_6_|_7_|_8_|_9_|...
IRCD_SIMD_TYPEDEF(c16x8,    c16, 16)  // [_0_|_1_|_2_|_3_|_4_|_5_|_6_|_7_]

#ifdef HAVE_CHAR8_T
IRCD_SIMD_TYPEDEF(c8x64,    c8,  64)  // [0|1|2|3|4|5|6|7|8|9|a|b|c|d|e|f|0|1|2|3|4|5|6|7|...
IRCD_SIMD_TYPEDEF(c8x32,    c8,  32)  // [0|1|2|3|4|5|6|7|8|9|a|b|c|d|e|f|0|1|2|3|4...
IRCD_SIMD_TYPEDEF(c8x16,    c8,  16)  // [0|1|2|3|4|5|6|7|8|9|a|b|c|d|e|f]
#endif

//
// floating point precision
//

IRCD_SIMD_TYPEUSE(f512x1, m512f, 64)  // [_______________________________0________________...
IRCD_SIMD_TYPEUSE(f256x1, m256f, 32)  // [________________0________________|
IRCD_SIMD_TYPEUSE(f128x1, m128f, 16)  // [________0________|

IRCD_SIMD_TYPEUSE(d512x1, m512d, 64)  // [_______________________________0________________...
IRCD_SIMD_TYPEUSE(d256x1, m256d, 32)  // [________________0________________|
IRCD_SIMD_TYPEUSE(d128x1, m128d, 16)  // [________0________|

IRCD_SIMD_TYPEDEF(f64x8,    f64, 64)  // + [____0____|____1____|____2____|____3____|____4___...
IRCD_SIMD_TYPEDEF(f64x4,    f64, 32)  // + [____0____|____1____|____2____|...
IRCD_SIMD_TYPEDEF(f64x2,    f64, 16)  // + [____0____|____1____]

IRCD_SIMD_TYPEDEF(f32x16,   f32, 64)  // + [__0__|__1__|__2__|__3__|__4__|__5__|__6__|__7__|...
IRCD_SIMD_TYPEDEF(f32x8,    f32, 32)  // + [__0__|__1__|__2__|__3__|__4__|...
IRCD_SIMD_TYPEDEF(f32x4,    f32, 16)  // + [__0__|__1__|__2__|__3__]

#if defined(HAVE_FP16)
IRCD_SIMD_TYPEDEF(f16x32,   f16, 64)  // - [_0_|_1_|_2_|_3_|_4_|_5_|_6_|_7_|_8_|_9_|_a_|_b_|...
IRCD_SIMD_TYPEDEF(f16x16,   f16, 32)  // - [_0_|_1_|_2_|_3_|_4_|_5_|_6_|_7_|_8_|_9_|...
IRCD_SIMD_TYPEDEF(f16x8,    f16, 16)  // - [_0_|_1_|_2_|_3_|_4_|_5_|_6_|_7_]
#endif

#if defined(HAVE_FP8)
IRCD_SIMD_TYPEDEF(f8x64,    f8,  64)  // - [0|1|2|3|4|5|6|7|8|9|a|b|c|d|e|f|0|1|2|3|4|5|6|7|...
IRCD_SIMD_TYPEDEF(f8x32,    f8,  32)  // - [0|1|2|3|4|5|6|7|8|9|a|b|c|d|e|f|0|1|2|3|4...
IRCD_SIMD_TYPEDEF(f8x16,    f8,  16)  // + [0|1|2|3|4|5|6|7|8|9|a|b|c|d|e|f]
#endif

//
// other words
//

namespace ircd::simd
{
	struct pshuf_imm8;
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
