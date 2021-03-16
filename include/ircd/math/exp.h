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
#define HAVE_IRCD_MATH_EXP_H

namespace ircd::math
{
	template<class T>
	typename std::enable_if<simd::is<T>(), T>::type
	exp(T);

	template<class T>
	typename std::enable_if<simd::is<T>(), T>::type
	expf(T);

	template<class T>
	typename std::enable_if<simd::is<T>(), T>::type
	expl(T);
}

namespace ircd
{
	using ::exp;
	using ::expf;
	using ::expl;
	using math::exp;
	using math::expf;
	using math::expl;
}

template<class T>
inline typename std::enable_if<ircd::simd::is<T>(), T>::type
ircd::math::expl(T a)
{
	for(uint i(0); i < simd::lanes<T>(); ++i)
		a[i] = ::expl(a[i]);

	return a;
}

template<class T>
inline typename std::enable_if<ircd::simd::is<T>(), T>::type
ircd::math::expf(T a)
{
	for(uint i(0); i < simd::lanes<T>(); ++i)
		a[i] = ::expf(a[i]);

	return a;
}

template<class T>
inline typename std::enable_if<ircd::simd::is<T>(), T>::type
ircd::math::exp(T a)
{
	for(uint i(0); i < simd::lanes<T>(); ++i)
		a[i] = std::exp(a[i]);

	return a;
}
