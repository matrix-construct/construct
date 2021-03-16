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
#define HAVE_IRCD_MATH_NORM_H

namespace ircd::math
{
	template<class T,
	         class D>
	void
	norm(const vector_view<T> &out,
	     const vector_view<const T> &in,
	     const simd::lane_type<T> &eps,
	     const vector_view<D> &tmp);
}

/// Renormalization.
///
/// TODO: Figure out how to use less double
template<class T,
         class D>
inline void
ircd::math::norm(const vector_view<T> &__restrict__ out,
                 const vector_view<const T> &__restrict__ in,
                 const simd::lane_type<T> &epsilon,
                 const vector_view<D> &tmp)
{
	using singlep = simd::lane_type<T>;
	using doublep = simd::lane_type<D>;

	static_assert(simd::lanes<T>() == simd::lanes<D>());
	assert(out.size() >= in.size());
	assert(tmp.size() >= in.size());

	const size_t num
	{
		in.size()
	};

	const size_t elems
	{
		num * simd::lanes<T>()
	};

	const doublep mean
	(
		//TODO: XXX
		math::mean<singlep, doublep>(vector_view<const singlep>
		{
			reinterpret_cast<const singlep *>(in.data()), elems
		})
	);

	for(uint i(0); i < num; ++i)
	{
		const auto wider
		{
			lane_cast<D>(in[i])
		};

		tmp[i] = wider - mean;

		const auto res
		{
			pow(tmp[i], 2)
		};

		out[i] = lane_cast<T>(res);
	}

	const doublep s
	(
		//TODO: XXX
		math::mean<singlep, doublep>(vector_view<const singlep>
		{
			reinterpret_cast<const singlep *>(out.data()), elems
		})
	);

	const doublep divisor
	(
		sqrt(s + epsilon)
	);

	for(uint i(0); i < num; ++i)
	{
		const auto res
		{
			tmp[i] / divisor
		};

		out[i] = lane_cast<T>(res);
	}
}
