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
	float
	last,     ///< Last addend.
	mean,     ///< Computed mean.
	sum[4];   ///< Summand spread. TODO XXX
};

#ifdef __OPENCL_C_VERSION__
/// Compute average of all elements in the input. The result is broadcast
/// to all elements of the output.
///
/// provide:
/// li = local thread id
/// ln = local group size
///
inline void
ircd_simt_math_mean_f4lldr(__local float4 *const restrict out,
                           __local const float4 *const restrict in)
{
	const uint
	li = get_local_id(0),
	ln = get_local_size(0);

	out[li] = in[li];
	ircd_simt_reduce_add_f4lldr(out);

	if(li == 0)
		out[li] = ircd_simt_reduce_add_f4(out[li]) / (ln * 4);

	ircd_simt_broadcast_f4lldr(out);
}
#endif
