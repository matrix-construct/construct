// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2021 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

/// Normalize the input, placing the result in possibly overlapping output.
/// This procedure requires an additional temporary buffer.
inline void
ircd_simt_math_norm_f4lldr(__local float4 *const out,
                           __local const float4 *const in,
                           __local float4 *const restrict tmp,
                           const uint num,
                           const uint i)
{
	ircd_simt_math_mean_f4lldr(tmp, in, num, i);

	const float4
	sub_mean = in[i] - tmp[i];

	tmp[i] = pow(sub_mean, 2);
	ircd_simt_math_mean_f4lldr(out, tmp, num, i);

	const float4
	epsilon = 0.00001f,
	s = sqrt(out[i] + epsilon);

	out[i] = sub_mean / s;
}
