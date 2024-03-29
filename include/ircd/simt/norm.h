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
#define HAVE_IRCD_SIMT_NORM_H

#ifdef __OPENCL_VERSION__
/// Normalize the input, placing the result in possibly overlapping output.
/// This procedure requires an additional temporary buffer.
inline void
ircd_simt_math_norm_f4lldr(__local float4 *const out,
                           __local const float4 *const in,
                           __local float4 *const restrict tmp,
                           const uint ln,
                           const uint li)
{
	tmp[li] = in[li];
	ircd_simt_math_mean_f4lldr(tmp, ln, li);

	const float4
	sub_mean = in[li] - tmp[li],
	sub_mean_sqr = pow(sub_mean, 2);

	out[li] = sub_mean_sqr;
	ircd_simt_math_mean_f4lldr(out, ln, li);

	const float4
	epsilon = FLT_EPSILON,
	s = native_sqrt(out[li] + epsilon),
	res = sub_mean / s;

	if(out == in)
		barrier(CLK_LOCAL_MEM_FENCE);

	out[li] = res;
}
#endif
