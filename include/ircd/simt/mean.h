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
#define HAVE_IRCD_SIMT_MEAN_H

/// Averaging state; this is for computing running averages
/// XXX eventually
struct ircd_math_mean
{
	/// Summand spread. TODO XXX
	float sum[4];

	/// Divisor.
	uint div;

	/// Last addend.
	float last;

	/// Computed mean.
	float mean;
};

#ifdef __OPENCL_VERSION__
/// Clear the `ircd_math_mean` state to zero.
inline void
ircd_simt_math_mean_clear(__local struct ircd_math_mean *const mean)
{
    mean->sum[0] = 0.0f;
    mean->sum[1] = 0.0f;
    mean->sum[2] = 0.0f;
    mean->sum[3] = 0.0f;
    mean->div = 0;
    mean->last = 0.0f;
    mean->mean = 0.0f;
}
#endif

#ifdef __OPENCL_VERSION__
/// Compute average of all elements in the input. The result is broadcast
/// to all elements of the output.
///
/// provide:
/// li = local thread id
/// ln = local group size
///
inline void
ircd_simt_math_mean_f4lldr(__local float4 *const buf,
                           const uint ln,
                           const uint li)
{
	ircd_simt_reduce_add_f4lldr(buf, ln, li);

	if(li == 0)
	{
		const float
		sum = ircd_simt_reduce_add_f4(buf[li]),
		div = ln * 4,
		res = sum / div;

		buf[li] = res;
	}

	ircd_simt_broadcast_f4lldr(buf, ln, li);
}
#endif
