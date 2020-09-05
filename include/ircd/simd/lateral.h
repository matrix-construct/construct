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
#define HAVE_IRCD_SIMD_LATERAL_H

namespace ircd::simd
{
	/// Perform a horizontal operation among lanes. The operation is specified
	/// by the caller who supplies a functor template like `std::bit_or` or
	/// `std::plus` etc. The result resides in lane[0] of the return vector
	/// while all other lanes of the return vector are undefined/junk as far
	/// as the caller is concerned.
	///
	/// This operation is intended to "reduce" or "collapse" a vector to a
	/// scalar value generally to make some control transfer etc. It does not
	/// necessitate a scalar result so it can be integrated into a sequence of
	/// vector operations without loss of purity. But of course, this operation
	/// is not efficient (crossing lanes never really is) and this template
	/// will output some log2 number of instructions. Using larger lane widths
	/// (i.e u64 rather than u8) can decrease the number of operations.
	///
	template<template<class>
	         class op,
	         class T>
	T lateral(T) noexcept = delete;

	template<template<class> class op> u8x16 lateral(u8x16) noexcept;
	template<template<class> class op> u8x32 lateral(u8x32) noexcept;
	template<template<class> class op> u8x64 lateral(u8x64) noexcept;

	template<template<class> class op> u16x8 lateral(u16x8) noexcept;
	template<template<class> class op> u16x16 lateral(u16x16) noexcept;
	template<template<class> class op> u16x32 lateral(u16x32) noexcept;

	template<template<class> class op> u32x4 lateral(u32x4) noexcept;
	template<template<class> class op> u32x8 lateral(u32x8) noexcept;
	template<template<class> class op> u32x16 lateral(u32x16) noexcept;

	template<template<class> class op> u64x2 lateral(u64x2) noexcept;
	template<template<class> class op> u64x4 lateral(u64x4) noexcept;
	template<template<class> class op> u64x8 lateral(u64x8) noexcept;
}

template<template<class>
         class op>
inline ircd::u64x8
ircd::simd::lateral(u64x8 a)
noexcept
{
	u64x4 b, c;
	for(size_t i(0); i < 4; ++i)
		b[i] = a[i], c[i] = a[i + 4];

	b = op{}(b, c);
	b = lateral<op>(b);
	a[0] = b[0];
	return a;
}

template<template<class>
         class op>
inline ircd::u64x4
ircd::simd::lateral(u64x4 a)
noexcept
{
	u64x2 b, c;
	for(size_t i(0); i < 2; ++i)
		b[i] = a[i], c[i] = a[i + 2];

	b = op{}(b, c);
	b = lateral<op>(b);
	a[0] = b[0];
	return a;
}

template<template<class>
         class op>
inline ircd::u64x2
ircd::simd::lateral(u64x2 a)
noexcept
{
	const u64x2 b
	{
		a[1], a[0]
	};

	a = op{}(a, b);
	return a;
}

template<template<class>
         class op>
inline ircd::u32x16
ircd::simd::lateral(u32x16 a)
noexcept
{
	u32x8 b, c;
	for(size_t i(0); i < 8; ++i)
		b[i] = a[i], c[i] = a[i + 8];

	b = op{}(b, c);
	b = lateral<op>(b);
	a[0] = b[0];
	return a;
}

template<template<class>
         class op>
inline ircd::u32x8
ircd::simd::lateral(u32x8 a)
noexcept
{
	u32x4 b, c;
	for(size_t i(0); i < 4; ++i)
		b[i] = a[i], c[i] = a[i + 4];

	b = op{}(b, c);
	b = lateral<op>(b);
	a[0] = b[0];
	return a;
}

template<template<class>
         class op>
inline ircd::u32x4
ircd::simd::lateral(u32x4 a)
noexcept
{
	u32x4 b
	{
		a[2], a[3], 0, 0,
	};

	a = op{}(a, b);
	b[0] = a[1];
	a = op{}(a, b);
	return a;
}

template<template<class>
         class op>
inline ircd::u16x32
ircd::simd::lateral(u16x32 a)
noexcept
{
	u16x16 b, c;
	for(size_t i(0); i < 16; ++i)
		b[i] = a[i], c[i] = a[i + 16];

	b = op{}(b, c);
	b = lateral<op>(b);
	a[0] = b[0];
	return a;
}

template<template<class>
         class op>
inline ircd::u16x16
ircd::simd::lateral(u16x16 a)
noexcept
{
	u16x8 b, c;
	for(size_t i(0); i < 8; ++i)
		b[i] = a[i], c[i] = a[i + 8];

	b = op{}(b, c);
	b = lateral<op>(b);
	a[0] = b[0];
	return a;
}

template<template<class>
         class op>
inline ircd::u16x8
ircd::simd::lateral(u16x8 a)
noexcept
{
	u16x8 b
	{
		a[4], a[5], a[6], a[7]
	};

	a = op{}(a, b);
	b[0] = a[2];
	b[1] = a[3];
	a = op{}(a, b);
	b[0] = a[1];
	a = op{}(a, b);
	return a;
}

template<template<class>
         class op>
inline ircd::u8x64
ircd::simd::lateral(u8x64 a)
noexcept
{
	u8x32 b, c;
	for(size_t i(0); i < 32; ++i)
		b[i] = a[i], c[i] = a[i + 32];

	b = op{}(b, c);
	b = lateral<op>(b);
	a[0] = b[0];
	return a;
}

template<template<class>
         class op>
inline ircd::u8x32
ircd::simd::lateral(u8x32 a)
noexcept
{
	u8x16 b, c;
	for(size_t i(0); i < 16; ++i)
		b[i] = a[i], c[i] = a[i + 16];

	b = op{}(b, c);
	b = lateral<op>(b);
	a[0] = b[0];
	return a;
}

template<template<class>
         class op>
inline ircd::u8x16
ircd::simd::lateral(u8x16 a)
noexcept
{
	u8x16 b
	{
		a[0x8], a[0x9], a[0xa], a[0xb], a[0xc], a[0xd], a[0xe], a[0xf],
	};

	a = op{}(a, b);
	b = u8x16
	{
		a[0x4], a[0x5], a[0x6], a[0x7],
	};

	a = op{}(a, b);
	b[0x0] = a[0x2];
	b[0x1] = a[0x3];
	a = op{}(a, b);
	b[0x0] = a[0x1];
	a = op{}(a, b);
	return a;
}
