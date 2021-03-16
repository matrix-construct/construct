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
#define HAVE_IRCD_MATH_POW_H

namespace ircd::math
{
	template<class T,
	         class E>
	typename std::enable_if<simd::is<T>(), T>::type
	pow(T, const E);

	template<class T,
	         class E>
	typename std::enable_if<simd::is<T>(), T>::type
	powf(T, const E);

	template<class T,
	         class E>
	typename std::enable_if<simd::is<T>(), T>::type
	powl(T, const E);
}

namespace ircd
{
	using ::pow;
	using ::powf;
	using ::powl;
	using math::pow;
	using math::powf;
	using math::powl;
}

template<class T,
         class E>
inline typename std::enable_if<ircd::simd::is<T>(), T>::type
ircd::math::powl(T a,
                 const E e)
{
	for(uint i(0); i < simd::lanes<T>(); ++i)
		a[i] = ::powl(a[i], e);

	return a;
}

template<class T,
         class E>
inline typename std::enable_if<ircd::simd::is<T>(), T>::type
ircd::math::powf(T a,
                 const E e)
{
	for(uint i(0); i < simd::lanes<T>(); ++i)
		a[i] = ::powf(a[i], e);

	return a;
}

template<class T,
         class E>
inline typename std::enable_if<ircd::simd::is<T>(), T>::type
ircd::math::pow(T a,
                const E e)
{
	for(uint i(0); i < simd::lanes<T>(); ++i)
		a[i] = std::pow(a[i], e);

	return a;
}
