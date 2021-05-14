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
#define HAVE_IRCD_MATH_SMAX_H

namespace ircd::math
{
	template<class T,
	         class D = T>
	typename std::enable_if<!simd::is<T>(), void>::type
	smax(const vector_view<T> &out,
	     const vector_view<const T> &in,
	     const vector_view<D> &tmp0,
	     const vector_view<D> &tmp1);

	template<class T,
	         class D = T>
	typename std::enable_if<!simd::is<T>(), void>::type
	smax(const vector_view<T> &out,
	     const vector_view<const T> &in,
	     const vector_view<D> &tmp);

	template<class T,
	         class D = T>
	typename std::enable_if<simd::is<T>(), void>::type
	smax(const vector_view<T> &out,
	     const vector_view<const T> &in,
	     const vector_view<D> &tmp,
	     const vector_view<D> &tmp1);

	template<class T,
	         class D = T>
	typename std::enable_if<simd::is<T>(), void>::type
	smax(const vector_view<T> &out,
	     const vector_view<const T> &in,
	     const vector_view<D> &tmp);
}

/// Softmax
template<class T,
         class D>
inline typename std::enable_if<ircd::simd::is<T>(), void>::type
ircd::math::smax(const vector_view<T> &__restrict__ out,
                 const vector_view<const T> &__restrict__ in,
                 const vector_view<D> &__restrict__ acc)
{
	assert(out.size() >= in.size());
	assert(acc.size() >= in.size());

	for(uint i(0); i < size(in); ++i)
		acc[i] = {0};

	for(uint i(0); i < size(in); ++i)
	{
		const auto precise
		{
			exp(lane_cast<D>(in[i]))
		};

		for(uint j(0); j < size(in); ++j)
			for(uint k(0); k < simd::lanes<D>(); ++k)
				acc[j] += precise[k];
	}

	for(uint i(0); i < size(in); ++i)
	{
		const auto precise
		{
			exp(lane_cast<D>(in[i]))
		};

		out[i] = lane_cast<T>(precise / acc[i]);
	}
}

/// Softmax
template<class T,
         class D>
inline typename std::enable_if<ircd::simd::is<T>(), void>::type
ircd::math::smax(const vector_view<T> &__restrict__ out,
                 const vector_view<const T> &__restrict__ in,
                 const vector_view<D> &__restrict__ acc,
                 const vector_view<D> &__restrict__ exps)
{
	assert(out.size() >= in.size());
	assert(acc.size() >= in.size());
	assert(exps.size() >= in.size());

	for(uint i(0); i < size(in); ++i)
		acc[i] = D{0};

	for(uint i(0); i < size(in); ++i)
		exps[i] = exp(lane_cast<D>(in[i]));

	for(uint i(0); i < size(in); ++i)
		for(uint j(0); j < size(in); ++j)
			for(uint k(0); k < simd::lanes<D>(); ++k)
				acc[j] += exps[i][k];

	for(uint i(0); i < size(in); ++i)
		out[i] = lane_cast<T>(exps[i] / acc[i]);
}

/// Softmax
template<class T,
         class D>
inline typename std::enable_if<!ircd::simd::is<T>(), void>::type
ircd::math::smax(const vector_view<T> &__restrict__ out,
                 const vector_view<const T> &__restrict__ in,
                 const vector_view<D> &__restrict__ acc)
{
	using ircd::exp;

	assert(out.size() >= in.size());
	assert(acc.size() >= in.size());

	for(uint i(0); i < size(in); ++i)
		acc[i] = D{0};

	for(uint i(0); i < size(in); ++i)
		out[i] = exp(in[i]);

	for(uint i(0); i < size(in); ++i)
		for(uint j(0); j < size(in); ++j)
			acc[j] += out[i];

	for(uint i(0); i < size(in); ++i)
		out[i] = out[i] / acc[i];
}

/// Softmax
template<class T,
         class D>
inline typename std::enable_if<!ircd::simd::is<T>(), void>::type
ircd::math::smax(const vector_view<T> &__restrict__ out,
                 const vector_view<const T> &__restrict__ in,
                 const vector_view<D> &__restrict__ acc,
                 const vector_view<D> &__restrict__ exps)
{
	using ircd::exp;

	assert(out.size() >= in.size());
	assert(acc.size() >= in.size());
	assert(exps.size() >= in.size());

	for(uint i(0); i < size(in); ++i)
		acc[i] = {0};

	for(uint i(0); i < size(in); ++i)
		exps[i] = exp(in[i]);

	for(uint i(0); i < size(in); ++i)
		for(uint j(0); j < size(in); ++j)
			acc[j] += exps[i];

	for(uint i(0); i < size(in); ++i)
		out[i] = exps[i] / acc[i];
}
