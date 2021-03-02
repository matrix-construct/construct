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
	template<class T>
	typename std::enable_if<!simd::is<T>(), T>::type
	mean(const vector_view<const T> &);

	template<class T>
	typename std::enable_if<simd::is<T>(), simd::lane_type<T>>::type
	mean(const vector_view<const T> &);
}

template<class T>
inline typename std::enable_if<ircd::simd::is<T>(), ircd::simd::lane_type<T>>::type
ircd::math::mean(const vector_view<const T> &a)
{
	using value_type = simd::lane_type<T>;

	const auto &sum
	{
		simd::accumulate(a.data(), u64x2{0, a.size()}, T{0}, []
		(auto &ret, const auto block, const auto mask)
		{
			ret += block;
		})
	};

	value_type num {0};
	for(size_t i{0}; i < simd::lanes<T>(); ++i)
		num += sum[i];

	const auto &den
	{
		a.size() * simd::lanes<T>()
	};

	num /= den;
	return num;
}

template<class T>
inline typename std::enable_if<!ircd::simd::is<T>(), T>::type
ircd::math::mean(const vector_view<const T> &a)
{
	T ret{0};

	size_t i{0};
	while(i < a.size())
		ret += a[i++];

	ret /= i;
	return ret;
}
