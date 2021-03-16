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
#define HAVE_IRCD_MATH_TANH_H

namespace ircd::math
{
	template<class T>
	typename std::enable_if<simd::is<T>(), T>::type
	tanh(T);

	template<class T>
	typename std::enable_if<simd::is<T>(), T>::type
	tanhf(T);

	template<class T>
	typename std::enable_if<simd::is<T>(), T>::type
	tanhl(T);
}

namespace ircd
{
	using ::tanh;
	using ::tanhf;
	using ::tanhl;
	using math::tanh;
	using math::tanhf;
	using math::tanhl;
}

template<class T>
inline typename std::enable_if<ircd::simd::is<T>(), T>::type
ircd::math::tanhl(T a)
{
	for(uint i(0); i < simd::lanes<T>(); ++i)
		a[i] = ::tanhl(a[i]);

	return a;
}

template<class T>
inline typename std::enable_if<ircd::simd::is<T>(), T>::type
ircd::math::tanhf(T a)
{
	for(uint i(0); i < simd::lanes<T>(); ++i)
		a[i] = ::tanhf(a[i]);

	return a;
}

template<class T>
inline typename std::enable_if<ircd::simd::is<T>(), T>::type
ircd::math::tanh(T a)
{
	for(uint i(0); i < simd::lanes<T>(); ++i)
		a[i] = std::tanh(a[i]);

	return a;
}
