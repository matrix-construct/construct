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
#define HAVE_IRCD_MATH_FMMA_H

namespace ircd::math
{
	template<class T>
	void fmma(T *, const T *, const T *, const struct fmma_opts &);
}

/// Options for the template.
struct ircd::math::fmma_opts
{
	size_t cols     {   0 };
	size_t rows     {   0 };
	size_t tiles    {   1 };
};

/// Fused Matrix-Multiply & Accumulate
/// clang11 FMA vfmadd213ps/vfmadd231ps
/// clang11 FMA4 vfmaddps
template<class T>
inline void
ircd::math::fmma(T *const __restrict__ out,
                 const T *const __restrict__ in,
                 const T *const __restrict__ weight,
                 const struct fmma_opts &opts)
{
	const auto
	&cols{opts.cols},
	&rows{opts.rows},
	&tiles{opts.tiles},
	&lanes{simd::lanes<T>()};

	const auto width
	{
		cols / lanes / tiles
	};

	const auto height
	{
		rows / lanes
	};

	for(uint i(0); i < width; i++)
		for(uint j(0); j < height; j++)
			for(uint t(0); t < tiles; ++t)
				for(uint l(0); l < lanes; ++l)
				{
					const auto x
					{
						i * tiles + t
					};

					const auto y
					{
						x * lanes + l
					};

					const T mul
					{
						in[x][l] * weight[y * height + j]
					};

					out[j] += mul;
				}
}
