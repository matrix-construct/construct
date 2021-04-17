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
#define HAVE_IRCD_MATH_SQRT_H

namespace ircd::math
{
	template<class T>
	typename std::enable_if<simd::is<T>(), T>::type
	sqrt(T);

	template<class T>
	typename std::enable_if<simd::is<T>(), T>::type
	sqrtf(T);

	template<class T>
	typename std::enable_if<simd::is<T>(), T>::type
	sqrtl(T);
}

namespace ircd
{
	using ::sqrt;
	using ::sqrtf;
	using ::sqrtl;
	using math::sqrt;
	using math::sqrtf;
	using math::sqrtl;
}

template<class T>
inline typename std::enable_if<ircd::simd::is<T>(), T>::type
ircd::math::sqrtl(T a)
{
	for(uint i(0); i < simd::lanes<T>(); ++i)
		a[i] = ::sqrtl(a[i]);

	return a;
}

template<class T>
inline typename std::enable_if<ircd::simd::is<T>(), T>::type
ircd::math::sqrtf(T a)
{
	for(uint i(0); i < simd::lanes<T>(); ++i)
		a[i] = ::sqrtf(a[i]);

	return a;
}

template<class T>
inline typename std::enable_if<ircd::simd::is<T>(), T>::type
ircd::math::sqrt(T a)
{
	for(uint i(0); i < simd::lanes<T>(); ++i)
		a[i] = std::sqrt(a[i]);

	return a;
}
