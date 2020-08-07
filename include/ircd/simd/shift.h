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
#define HAVE_IRCD_SIMD_SHIFT_H

// xmmx shifter interface
namespace ircd::simd
{
	template<int b,
             class T>
	typename std::enable_if<sizeof(T) == 16, T>::type
	shl(const T a) noexcept;

	template<int b,
             class T>
	typename std::enable_if<sizeof(T) == 32, T>::type
	shl(const T a) noexcept;

	template<int b,
             class T>
	typename std::enable_if<sizeof(T) == 64, T>::type
	shl(const T a) noexcept;

	template<int b,
             class T>
	typename std::enable_if<sizeof(T) == 16, T>::type
	shr(const T a) noexcept;

	template<int b,
             class T>
	typename std::enable_if<sizeof(T) == 32, T>::type
	shr(const T a) noexcept;

	template<int b,
             class T>
	typename std::enable_if<sizeof(T) == 64, T>::type
	shr(const T a) noexcept;
}

// xmmx shifter fallback template (internal)
namespace ircd::simd
{
	template<int b,
             class V,
             class T>
	T _shl(const T) noexcept;

	template<int b,
	         class V,
             class T>
	T _shr(const T) noexcept;
}

#ifdef HAVE_X86INTRIN_H
template<int b,
         class T>
[[using gnu: always_inline, gnu_inline, artificial]]
extern inline typename std::enable_if<sizeof(T) == 16, T>::type
ircd::simd::shr(const T a)
noexcept
{
	static_assert
	(
		b % 8 == 0, "xmmx register only shifts right at bytewise resolution."
	);

	return T(_mm_bsrli_si128(u128x1(a), b / 8));
}
#endif

#ifdef HAVE_X86INTRIN_H
template<int b,
         class T>
[[using gnu: always_inline, gnu_inline, artificial]]
extern inline typename std::enable_if<sizeof(T) == 16, T>::type
ircd::simd::shl(const T a)
noexcept
{
	static_assert
	(
		b % 8 == 0, "xmmx register only shifts left at bytewise resolution."
	);

	return T(_mm_bslli_si128(u128x1(a), b / 8));
}
#endif

#if defined(HAVE_X86INTRIN_H) && defined(__AVX2__)
template<int b,
         class T>
[[using gnu: always_inline, gnu_inline, artificial]]
extern inline typename std::enable_if<sizeof(T) == 32, T>::type
ircd::simd::shr(const T a)
noexcept
{
	static_assert
	(
		b % 8 == 0, "ymmx register only shifts right at bytewise resolution."
	);

	return T(_mm256_srli_si256(u256x1(a), b / 8));
}
#else
template<int b,
         class T>
[[using gnu: always_inline, gnu_inline, artificial]]
extern inline typename std::enable_if<sizeof(T) == 32, T>::type
ircd::simd::shr(const T a)
noexcept
{
	return _shr<b, u8x32>(a);
}
#endif

#if defined(HAVE_X86INTRIN_H) && defined(__AVX2__)
template<int b,
         class T>
[[using gnu: always_inline, gnu_inline, artificial]]
extern inline typename std::enable_if<sizeof(T) == 32, T>::type
ircd::simd::shl(const T a)
noexcept
{
	static_assert
	(
		b % 8 == 0, "ymmx register only shifts left at bytewise resolution."
	);

	return T(_mm256_slli_si256(u256x1(a), b / 8));
}
#else
template<int b,
         class T>
[[using gnu: always_inline, gnu_inline, artificial]]
extern inline typename std::enable_if<sizeof(T) == 32, T>::type
ircd::simd::shl(const T a)
noexcept
{
	return _shl<b, u8x32>(a);
}
#endif

#if defined(HAVE_X86INTRIN_H) && defined(__AVX512F__)
template<int b,
         class T>
[[using gnu: always_inline, gnu_inline, artificial]]
extern inline typename std::enable_if<sizeof(T) == 64, T>::type
ircd::simd::shr(const T a)
noexcept
{
	static_assert
	(
		b % 8 == 0, "ymmx register only shifts right at bytewise resolution."
	);

	return T(_mm512_srli_si512(u512x1(a), b / 8));
}
#else
template<int b,
         class T>
[[using gnu: always_inline, gnu_inline, artificial]]
extern inline typename std::enable_if<sizeof(T) == 64, T>::type
ircd::simd::shr(const T a)
noexcept
{
	return _shr<b, u8x64>(a);
}
#endif

#if defined(HAVE_X86INTRIN_H) && defined(__AVX512F__)
template<int b,
         class T>
[[using gnu: always_inline, gnu_inline, artificial]]
extern inline typename std::enable_if<sizeof(T) == 64, T>::type
ircd::simd::shl(const T a)
noexcept
{
	static_assert
	(
		b % 8 == 0, "ymmx register only shifts left at bytewise resolution."
	);

	return T(_mm512_slli_si512(u512x1(a), b / 8));
}
#else
template<int b,
         class T>
[[using gnu: always_inline, gnu_inline, artificial]]
extern inline typename std::enable_if<sizeof(T) == 64, T>::type
ircd::simd::shl(const T a)
noexcept
{
	return _shl<b, u8x64>(a);
}
#endif

template<int b,
         class V,
         class T>
[[using gnu: always_inline, gnu_inline, artificial]]
extern inline T
ircd::simd::_shr(const T a)
noexcept
{
	static_assert
	(
		sizeof(T) == sizeof(V)
	);

	static_assert
	(
		b % 8 == 0, "[emulated] xmmx register only shifts right at bytewise resolution."
	);

	constexpr int B
	{
		b / 8
	};

	V ret;
	for(size_t i(0); i < sizeof(V) - B; ++i)
		ret[i] = a[i + B];

	for(size_t i(sizeof(V) - B); i < sizeof(V); ++i)
		ret[i] = 0;

	return T(ret);
}

template<int b,
         class V,
         class T>
[[using gnu: always_inline, gnu_inline, artificial]]
extern inline T
ircd::simd::_shl(const T a)
noexcept
{
	static_assert
	(
		sizeof(T) == sizeof(V)
	);

	static_assert
	(
		b % 8 == 0, "[emulated] xmmx register only shifts left at bytewise resolution."
	);

	constexpr int B
	{
		b / 8
	};

	V ret;
	for(size_t i(0); i < B; ++i)
		ret[i] = 0;

	for(size_t i(B); i < sizeof(V); ++i)
		ret[i] = a[i - B];

	return T(ret);
}
