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
#define HAVE_IRCD_SIMT_SORT_H

#ifdef __OPENCL_C_VERSION__
/// Sort indices in `idx` which point to values contained in `val`.
inline void
ircd_simt_sort_idx16_flldr(__local ushort *const idx,
                           __global const float *const val)
{
	const uint
	li = get_local_id(0),
	ln = get_local_size(0);

	for(uint stride = ln >> 1; stride > 0; stride >>= 1)
	{
		barrier(CLK_LOCAL_MEM_FENCE);
		if(li >= stride || val[idx[li]] >= val[idx[li + stride]])
			continue;

		const ushort
		ours = idx[li],
		theirs = idx[li + stride];

		idx[li] = theirs;
		idx[li + stride] = ours;
	}
}
#endif
