// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2022 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_MATH_DIFF_H

namespace ircd::math
{
	template<class T>
	typename std::enable_if<!simd::is<T>(), T>::type
	diff(const T, const T);

	template<class T>
	typename std::enable_if<simd::is<T>(), T>::type
	diff(const T, const T);
}

namespace ircd
{
	using math::diff;
}

template<class T>
inline typename std::enable_if<ircd::simd::is<T>(), T>::type
ircd::math::diff(const T a, const T b)
{
	T r;
	for(uint i(0); i < simd::lanes<T>(); ++i)
		r[i] = diff(a[i], b[i]);

	return r;
}

template<class T>
inline typename std::enable_if<!ircd::simd::is<T>(), T>::type
ircd::math::diff(const T a, const T b)
{
	return a > b?
		a - b:
		b - a;
}
