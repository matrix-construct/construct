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
#define HAVE_IRCD_SIMD_STR_H

// String toolset.
//
// Whole vectors are in play at a time, with null termination allowing for
// string ops at character granularity. A few basic rules:
//
// - Strings are laid onto vectors with character elements; the templates
// accept c8/i8/u8 in combination with vector sizes of 16, 32 and 64. This
// is not always addressable memory, so this is a value-oriented interface
// and uses no character pointers; only pointers to vectors (whole strings).
//
// - Strings are null terminated iff there is room for a null character. If
// a vector does not contain a null character the string's length is the
// vector size itself. It is up to the caller (or a derived interface) to
// compose long strings out of unterminated blocks.
//
// - Null termination *must* be padded out to the end of the vector. This is
// important for performance. Unless otherwise noted there is one string on
// a vector at a time and the first character is lane[0]
//
namespace ircd::simd
{
	template<class T>
	typename std::enable_if<simd::sizeof_lane<T>() == 1, u8>::type
	strlen(const T) noexcept;

	template<class T>
	typename std::enable_if<simd::sizeof_lane<T>() == 1, bool>::type
	streq(const T, const T) noexcept;

	template<class T>
	typename std::enable_if<simd::sizeof_lane<T>() == 1, T &>::type
	strcpy(T &, const T) noexcept;

	template<class T>
	typename std::enable_if<simd::sizeof_lane<T>() == 1, T &>::type
	strcpy(T &, const const_buffer &) noexcept;

	template<class T>
	typename std::enable_if<simd::sizeof_lane<T>() == 1, T &>::type
	strcat(T &, const T) noexcept;
}

template<class T>
inline typename std::enable_if<ircd::simd::sizeof_lane<T>() == 1, T &>::type
ircd::simd::strcat(T &a,
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

	while(i < lanes<T>())
		a[i++] = 0;

	return a;
}

template<class T>
inline typename std::enable_if<ircd::simd::sizeof_lane<T>() == 1, T &>::type
ircd::simd::strcpy(T &a,
                   const const_buffer &b)
noexcept
{
	static_assert
	(
		sizeof_lane<T>() == 1,
		"internal template; for 8-bit character basis only"
	);

	size_t i{0};
	for(; i < lanes<T>() && i < size(b); ++i)
		a[i] = b[i];

	while(i < lanes<T>())
		a[i++] = 0;

	return a;
}

template<class T>
inline typename std::enable_if<ircd::simd::sizeof_lane<T>() == 1, T &>::type
ircd::simd::strcpy(T &a,
                   const T b)
noexcept
{
	static_assert
	(
		sizeof_lane<T>() == 1,
		"internal template; for 8-bit character basis only"
	);

	size_t i{0};
	for(; b[i] && i < lanes<T>(); ++i)
		a[i] = b[i];

	while(i < lanes<T>())
		a[i++] = 0;

	return a;
}

template<class T>
inline typename std::enable_if<ircd::simd::sizeof_lane<T>() == 1, bool>::type
ircd::simd::streq(const T a,
                  const T b)
noexcept
{
	static_assert
	(
		sizeof_lane<T>() == 1,
		"internal template; for 8-bit character basis only"
	);

	const auto equal
	{
		a == b
	};

	return all<T>(equal);
}

template<class T>
inline typename std::enable_if<ircd::simd::sizeof_lane<T>() == 1, ircd::u8>::type
ircd::simd::strlen(const T str)
noexcept
{
	static_assert
	(
		sizeof_lane<T>() == 1,
		"internal template; for 8-bit character basis only"
	);

	u8 ret(0);
	for(; ret < lanes<T>(); ++ret)
		if(!str[ret])
			break;

	return ret;
}
