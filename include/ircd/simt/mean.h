// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2021 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

/// Compute average of all elements in the input. The result is broadcast
/// to all elements of the output.
inline void
ircd_simt_math_mean_f4lldr(__local float4 *const restrict out,
                           __local const float4 *const restrict in,
                           const uint num,
                           const uint i)
{
	out[i] = in[i];
	ircd_simt_reduce_add_f4lldr(out, num, i);

	if(i == 0)
		out[i][0] = ircd_simt_reduce_add_f4(out[i]);

	if(i == 0)
		out[i] = out[i][0] / (num * 4);

	ircd_simt_broadcast_f4lldr(out, num, i);
}
