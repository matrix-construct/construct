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
#define HAVE_IRCD_SIMD_ROR_H

// xmmx rotate-right interface
namespace ircd::simd
{
	template<int b,
	         class V,
	         class T>
	T _ror(const T) noexcept;

	template<int b,
	         class T>
	typename std::enable_if<sizeof(T) == 16, T>::type
	ror(const T a) noexcept;

	template<int b,
	         class T>
	typename std::enable_if<sizeof(T) == 32, T>::type
	ror(const T a) noexcept;

	template<int b,
	         class T>
	typename std::enable_if<sizeof(T) == 64, T>::type
	ror(const T a) noexcept;
}

template<int b,
         class V,
         class T>
[[using gnu: always_inline, gnu_inline, artificial]]
extern inline T
ircd::simd::_ror(const T a)
noexcept
{
	static_assert
	(
		sizeof(T) == sizeof(V)
	);

	static_assert
	(
		b % 8 == 0, "[emulated] xmmx register only rotates left at bytewise resolution."
	);

	constexpr int B
	{
		b / 8
	};

	constexpr auto S
	{
		sizeof(V) - B
	};

	size_t i(0);
	V arg(a), ret;
	for(; i < S; ++i)
		ret[i] = arg[i + B];

	for(; i < sizeof(V); ++i)
		ret[i] = arg[i - S];

	return T(ret);
}

template<int b,
         class T>
[[using gnu: always_inline, gnu_inline, artificial]]
extern inline typename std::enable_if<sizeof(T) == 16, T>::type
ircd::simd::ror(const T a)
noexcept
{
	return _ror<b, u8x16>(a);
}

template<int b,
         class T>
[[using gnu: always_inline, gnu_inline, artificial]]
extern inline typename std::enable_if<sizeof(T) == 32, T>::type
ircd::simd::ror(const T a)
noexcept
{
	return _ror<b, u8x32>(a);
}

template<int b,
         class T>
[[using gnu: always_inline, gnu_inline, artificial]]
extern inline typename std::enable_if<sizeof(T) == 64, T>::type
ircd::simd::ror(const T a)
noexcept
{
	return _ror<b, u8x64>(a);
}
