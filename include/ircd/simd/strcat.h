// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2021 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_SIMD_STRCAT_H

namespace ircd::simd
{
	template<class T>
	T &_strcat(T &, const T) noexcept;

	u8x16 &strcat(u8x16 &, const u8x16) noexcept;
	u8x32 &strcat(u8x32 &, const u8x32) noexcept;
	u8x64 &strcat(u8x64 &, const u8x64) noexcept;
}

inline ircd::simd::u8x64 &
ircd::simd::strcat(u8x64 &a,
                   const u8x64 b)
noexcept
{
	return _strcat(a, b);
}

inline ircd::simd::u8x32 &
ircd::simd::strcat(u8x32 &a,
                   const u8x32 b)
noexcept
{
	return _strcat(a, b);
}

inline ircd::simd::u8x16 &
ircd::simd::strcat(u8x16 &a,
                   const u8x16 b)
noexcept
{
	return _strcat(a, b);
}

template<class T>
inline T &
ircd::simd::_strcat(T &a,
                    const T b)
noexcept
{
	static_assert
	(
		sizeof_lane<T>() == 1,
		"internal template; for 8-bit character basis only"
	);

	size_t j{0}, i
	{
		strlen(a)
	};

	while(b[j] && i < lanes<T>())
		a[i++] = b[j++];

	return a;
}
