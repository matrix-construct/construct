// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for all
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_SIMD_ALL_H

namespace ircd::simd
{
	/// Convenience wrapper around reduce<std::bit_and>()[0] with the most
	/// efficient lane type conversions made.
	template<class T>
	bool all(const T) noexcept = delete;
}

template<>
inline bool
ircd::simd::all(const u64x8 a)
noexcept
{
	return reduce<std::bit_and>(a)[0] == -1UL;
}

template<>
inline bool
ircd::simd::all(const u64x4 a)
noexcept
{
	return reduce<std::bit_and>(a)[0] == -1UL;
}

template<>
inline bool
ircd::simd::all(const u64x2 a)
noexcept
{
	return reduce<std::bit_and>(a)[0] == -1UL;
}

template<>
inline bool
ircd::simd::all(const u32x16 a)
noexcept
{
	return all(u64x8(a));
}

template<>
inline bool
ircd::simd::all(const u32x8 a)
noexcept
{
	return all(u64x4(a));
}

template<>
inline bool
ircd::simd::all(const u32x4 a)
noexcept
{
	return all(u64x2(a));
}

template<>
inline bool
ircd::simd::all(const u16x32 a)
noexcept
{
	return all(u64x8(a));
}

template<>
inline bool
ircd::simd::all(const u16x16 a)
noexcept
{
	return all(u64x4(a));
}

template<>
inline bool
ircd::simd::all(const u16x8 a)
noexcept
{
	return all(u64x2(a));
}

template<>
inline bool
ircd::simd::all(const u8x64 a)
noexcept
{
	return all(u64x8(a));
}

template<>
inline bool
ircd::simd::all(const u8x32 a)
noexcept
{
	return all(u64x4(a));
}

template<>
inline bool
ircd::simd::all(const u8x16 a)
noexcept
{
	return all(u64x2(a));
}
