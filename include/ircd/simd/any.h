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
#define HAVE_IRCD_SIMD_ANY_H

namespace ircd::simd
{
	/// Convenience wrapper around reduce<std::bit_or>()[0] with the most
	/// efficient lane type conversions made.
	template<class T>
	bool any(const T) noexcept = delete;
}

template<>
inline bool
ircd::simd::any(const u64x8 a)
noexcept
{
	return reduce<std::bit_or>(a)[0];
}

template<>
inline bool
ircd::simd::any(const u64x4 a)
noexcept
{
	return reduce<std::bit_or>(a)[0];
}

template<>
inline bool
ircd::simd::any(const u64x2 a)
noexcept
{
	return reduce<std::bit_or>(a)[0];
}

template<>
inline bool
ircd::simd::any(const u32x16 a)
noexcept
{
	return any(u64x8(a));
}

template<>
inline bool
ircd::simd::any(const u32x8 a)
noexcept
{
	return any(u64x4(a));
}

template<>
inline bool
ircd::simd::any(const u32x4 a)
noexcept
{
	return any(u64x2(a));
}

template<>
inline bool
ircd::simd::any(const u16x32 a)
noexcept
{
	return any(u64x8(a));
}

template<>
inline bool
ircd::simd::any(const u16x16 a)
noexcept
{
	return any(u64x4(a));
}

template<>
inline bool
ircd::simd::any(const u16x8 a)
noexcept
{
	return any(u64x2(a));
}

template<>
inline bool
ircd::simd::any(const u8x64 a)
noexcept
{
	return any(u64x8(a));
}

template<>
inline bool
ircd::simd::any(const u8x32 a)
noexcept
{
	return any(u64x4(a));
}

template<>
inline bool
ircd::simd::any(const u8x16 a)
noexcept
{
	return any(u64x2(a));
}
