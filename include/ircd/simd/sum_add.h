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
#define HAVE_IRCD_SIMD_SUM_ADD_H

namespace ircd::simd
{
	template<class T> T sum_add(T) noexcept = delete;

	template<> u8x16 sum_add(u8x16) noexcept;
	template<> u8x32 sum_add(u8x32) noexcept;
	template<> u8x64 sum_add(u8x64) noexcept;

	template<> u16x8 sum_add(u16x8) noexcept;
	template<> u16x16 sum_add(u16x16) noexcept;
	template<> u16x32 sum_add(u16x32) noexcept;

	template<> u32x4 sum_add(u32x4) noexcept;
	template<> u32x8 sum_add(u32x8) noexcept;
	template<> u32x16 sum_add(u32x16) noexcept;

	template<> u64x2 sum_add(u64x2) noexcept;
	template<> u64x4 sum_add(u64x4) noexcept;
	template<> u64x8 sum_add(u64x8) noexcept;
}

template<>
inline ircd::u64x8
ircd::simd::sum_add(u64x8 a)
noexcept
{
	u64x4 b, c;
	for(size_t i(0); i < 4; ++i)
		b[i] = a[i], c[i] = a[i + 4];

	b += c;
	b = sum_add(b);
	a[0] = b[0];
	return a;
}

template<>
inline ircd::u64x4
ircd::simd::sum_add(u64x4 a)
noexcept
{
	u64x2 b, c;
	for(size_t i(0); i < 2; ++i)
		b[i] = a[i], c[i] = a[i + 2];

	b += c;
	b = sum_add(b);
	a[0] = b[0];
	return a;
}

template<>
inline ircd::u64x2
ircd::simd::sum_add(u64x2 a)
noexcept
{
	const u64x2 b
	{
		a[1], a[0]
	};

	a += b;
	return a;
}

template<>
inline ircd::u32x16
ircd::simd::sum_add(u32x16 a)
noexcept
{
	u32x8 b, c;
	for(size_t i(0); i < 8; ++i)
		b[i] = a[i], c[i] = a[i + 8];

	b += c;
	b = sum_add(b);
	a[0] = b[0];
	return a;
}

template<>
inline ircd::u32x8
ircd::simd::sum_add(u32x8 a)
noexcept
{
	u32x4 b, c;
	for(size_t i(0); i < 4; ++i)
		b[i] = a[i], c[i] = a[i + 4];

	b += c;
	b = sum_add(b);
	a[0] = b[0];
	return a;
}

template<>
inline ircd::u32x4
ircd::simd::sum_add(u32x4 a)
noexcept
{
	u32x4 b
	{
		a[2], a[3], 0, 0,
	};

	a = a + b;
	b[0] = a[1];
	a = a + b;
	return a;
}

template<>
inline ircd::u16x32
ircd::simd::sum_add(u16x32 a)
noexcept
{
	u16x16 b, c;
	for(size_t i(0); i < 16; ++i)
		b[i] = a[i], c[i] = a[i + 16];

	b += c;
	b = sum_add(b);
	a[0] = b[0];
	return a;
}

template<>
inline ircd::u16x16
ircd::simd::sum_add(u16x16 a)
noexcept
{
	u16x8 b, c;
	for(size_t i(0); i < 8; ++i)
		b[i] = a[i], c[i] = a[i + 8];

	b += c;
	b = sum_add(b);
	a[0] = b[0];
	return a;
}

template<>
inline ircd::u16x8
ircd::simd::sum_add(u16x8 a)
noexcept
{
	u16x8 b
	{
		a[4], a[5], a[6], a[7]
	};

	a = a + b;
	b[0] = a[2];
	b[1] = a[3];
	a = a + b;
	b[0] = a[1];
	a = a + b;
	return a;
}

template<>
inline ircd::u8x64
ircd::simd::sum_add(u8x64 a)
noexcept
{
	u8x32 b, c;
	for(size_t i(0); i < 32; ++i)
		b[i] = a[i], c[i] = a[i + 32];

	b += c;
	b = sum_add(b);
	a[0] = b[0];
	return a;
}

template<>
inline ircd::u8x32
ircd::simd::sum_add(u8x32 a)
noexcept
{
	u8x16 b, c;
	for(size_t i(0); i < 16; ++i)
		b[i] = a[i], c[i] = a[i + 16];

	b += c;
	b = sum_add(b);
	a[0] = b[0];
	return a;
}

template<>
inline ircd::u8x16
ircd::simd::sum_add(u8x16 a)
noexcept
{
	u8x16 b
	{
		a[0x8], a[0x9], a[0xa], a[0xb], a[0xc], a[0xd], a[0xe], a[0xf],
	};

	a = a + b;
	b = u8x16
	{
		a[0x4], a[0x5], a[0x6], a[0x7],
	};

	a = a + b;
	b[0x0] = a[0x2];
	b[0x1] = a[0x3];
	a = a + b;
	b[0x0] = a[0x1];
	a = a + b;
	return a;
}
