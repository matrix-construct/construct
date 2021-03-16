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
#define HAVE_IRCD_SIMD_HADD_H

// Horizontal add
namespace ircd::simd
{
	template<class T,
	         class R = T>
	typename std::enable_if<simd::lanes<T>() == 2, R>::type
	hadd(const T, const T) noexcept;

	template<class T,
	         class R = T>
	typename std::enable_if<simd::lanes<T>() == 4, R>::type
	hadd(const T, const T) noexcept;

	template<class T,
	         class R = T>
	typename std::enable_if<simd::lanes<T>() == 8, R>::type
	hadd(const T, const T) noexcept;

	template<class T,
	         class R = T>
	typename std::enable_if<simd::lanes<T>() == 16, R>::type
	hadd(const T, const T) noexcept;
}

template<class T,
         class R>
inline typename std::enable_if<ircd::simd::lanes<T>() == 16, R>::type
ircd::simd::hadd(const T a,
                 const T b)
noexcept
{
	return R
	{
		a[0x01] + a[0x00],
		a[0x03] + a[0x02],
		a[0x05] + a[0x04],
		a[0x07] + a[0x06],
		b[0x01] + b[0x00],
		b[0x03] + b[0x02],
		b[0x05] + b[0x04],
		b[0x07] + b[0x06],
		a[0x09] + a[0x08],
		a[0x0b] + a[0x0a],
		a[0x0d] + a[0x0c],
		a[0x0f] + a[0x0e],
		b[0x09] + b[0x08],
		b[0x0b] + b[0x0a],
		b[0x0d] + b[0x0c],
		b[0x0f] + b[0x0e],
	};
}

template<class T,
         class R>
inline typename std::enable_if<ircd::simd::lanes<T>() == 8, R>::type
ircd::simd::hadd(const T a,
                 const T b)
noexcept
{
	return R
	{
		a[0x01] + a[0x00],
		a[0x03] + a[0x02],
		b[0x01] + b[0x00],
		b[0x03] + b[0x02],
		a[0x05] + a[0x04],
		a[0x07] + a[0x06],
		b[0x05] + b[0x04],
		b[0x07] + b[0x06],
	};
}

template<class T,
         class R>
inline typename std::enable_if<ircd::simd::lanes<T>() == 4, R>::type
ircd::simd::hadd(const T a,
                 const T b)
noexcept
{
	return R
	{
		a[0x01] + a[0x00],
		a[0x03] + a[0x02],
		b[0x01] + b[0x00],
		b[0x03] + b[0x02],
	};
}

template<class T,
         class R>
inline typename std::enable_if<ircd::simd::lanes<T>() == 2, R>::type
ircd::simd::hadd(const T a,
                 const T b)
noexcept
{
	return R
	{
		a[0x01] + a[0x00],
		b[0x01] + b[0x00],
	};
}
