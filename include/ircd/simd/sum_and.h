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
	template<class T> T sum_and(T) noexcept = delete;

	template<> u64x2 sum_and(u64x2) noexcept;
	template<> u64x4 sum_and(u64x4) noexcept;
	template<> u64x8 sum_and(u64x8) noexcept;

	template<> u16x8 sum_and(u16x8) noexcept;
	template<> u16x16 sum_and(u16x16) noexcept;
	template<> u16x32 sum_and(u16x32) noexcept;

	template<> u32x4 sum_and(u32x4) noexcept;
	template<> u32x8 sum_and(u32x8) noexcept;
	template<> u32x16 sum_and(u32x16) noexcept;

	template<> u8x16 sum_and(u8x16) noexcept;
	template<> u8x32 sum_and(u8x32) noexcept;
	template<> u8x64 sum_and(u8x64) noexcept;
}

template<>
inline ircd::u8x64
ircd::simd::sum_and(u8x64 a)
noexcept
{
	u8x32 b, c;
	for(size_t i(0); i < 32; ++i)
		b[i] = a[i], c[i] = a[i + 32];

	b &= c;
	b = sum_and(b);
	a[0] = b[0];
	return a;
}

template<>
inline ircd::u8x32
ircd::simd::sum_and(u8x32 a)
noexcept
{
	u8x16 b, c;
	for(size_t i(0); i < 16; ++i)
		b[i] = a[i], c[i] = a[i + 16];

	b &= c;
	b = sum_and(b);
	a[0] = b[0];
	return a;
}

template<>
inline ircd::u8x16
ircd::simd::sum_and(u8x16 a)
noexcept
{
	a &= shr<64>(a);
	a &= shr<32>(a);
	a &= shr<16>(a);
	a &= shr<8>(a);
	return a;
}

template<>
inline ircd::u16x32
ircd::simd::sum_and(u16x32 a)
noexcept
{
	u16x16 b, c;
	for(size_t i(0); i < 16; ++i)
		b[i] = a[i], c[i] = a[i + 16];

	b &= c;
	b = sum_and(b);
	a[0] = b[0];
	return a;
}

template<>
inline ircd::u16x16
ircd::simd::sum_and(u16x16 a)
noexcept
{
	u16x8 b, c;
	for(size_t i(0); i < 8; ++i)
		b[i] = a[i], c[i] = a[i + 8];

	b &= c;
	b = sum_and(b);
	a[0] = b[0];
	return a;
}

template<>
inline ircd::u16x8
ircd::simd::sum_and(u16x8 a)
noexcept
{
	u32x4 b(a);
	b = sum_and(b);
	a[0] &= u16x8(b)[1];
	return a;
}

template<>
inline ircd::u32x16
ircd::simd::sum_and(u32x16 a)
noexcept
{
	u32x8 b, c;
	for(size_t i(0); i < 8; ++i)
		b[i] = a[i], c[i] = a[i + 8];

	b &= c;
	b = sum_and(b);
	a[0] = b[0];
	return a;
}

template<>
inline ircd::u32x8
ircd::simd::sum_and(u32x8 a)
noexcept
{
	u32x4 b, c;
	for(size_t i(0); i < 4; ++i)
		b[i] = a[i], c[i] = a[i + 4];

	b &= c;
	b = sum_and(b);
	a[0] = b[0];
	return a;
}

template<>
inline ircd::u32x4
ircd::simd::sum_and(u32x4 a)
noexcept
{
	u64x2 b(a);
	b = sum_and(b);
	a[0] &= u32x4(b)[1];
	return a;
}

template<>
inline ircd::u64x8
ircd::simd::sum_and(u64x8 a)
noexcept
{
	u64x4 b, c;
	for(size_t i(0); i < 4; ++i)
		b[i] = a[i], c[i] = a[i + 4];

	b &= c;
	b = sum_and(b);
	a[0] = b[0];
	return a;
}

template<>
inline ircd::u64x4
ircd::simd::sum_and(u64x4 a)
noexcept
{
	u64x2 b, c;
	for(size_t i(0); i < 2; ++i)
		b[i] = a[i], c[i] = a[i + 2];

	b &= c;
	b = sum_and(b);
	a[0] = b[0];
	return a;
}

template<>
inline ircd::u64x2
ircd::simd::sum_and(u64x2 a)
noexcept
{
	u128x1 b(a);
	b = shr<64>(b);
	a &= u64x2(b);
	return a;
}
