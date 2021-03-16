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
#define HAVE_IRCD_MATH_MEAN_H

namespace ircd::math
{
	template<class T,
	         class R = T>
	typename std::enable_if<!simd::is<T>(), R>::type
	mean(const vector_view<const T>);

	template<class T,
	         class R = T>
	typename std::enable_if<simd::is<T>(), simd::lane_type<R>>::type
	mean(const vector_view<const T>);
}

namespace ircd
{
	using math::mean;
}

template<class T,
         class R>
inline typename std::enable_if<ircd::simd::is<T>(), ircd::simd::lane_type<R>>::type
ircd::math::mean(const vector_view<const T> a)
{
	R acc {0};
	simd::for_each(a.data(), u64x2{0, a.size()}, [&acc]
	(const auto block, const auto mask)
	{
		const R dp
		(
			simd::lane_cast<R>(block)
		);

		acc += dp;
	});

	auto num(acc[0]);
	for(uint i(1); i < simd::lanes<T>(); ++i)
		num += acc[i];

	const auto den
	{
		a.size() * simd::lanes<T>()
	};

	num /= den;
	return num;
}

template<class T,
         class R>
inline typename std::enable_if<!ircd::simd::is<T>(), R>::type
ircd::math::mean(const vector_view<const T> a)
{
	R ret{0};
	size_t i{0};
	while(i < a.size())
		ret += a[i++];

	ret /= i;
	return ret;
}
