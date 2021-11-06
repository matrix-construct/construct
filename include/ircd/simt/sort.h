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

inline bool
ircd_simt_sort_idx16_cmpxchg(__local ushort *const idx,
                             __global const float *const val,
                             const uint ai,
                             const uint bi,
                             const bool parity)
{
	const ushort
	a = idx[ai],
	b = idx[bi];

	const bool
	lt = val[a] < val[b],
	swap = (lt && !parity) || (!lt && parity);

	if(swap)
	{
		idx[ai] = b;
		idx[bi] = a;
	}

	return swap;
}

inline bool
ircd_simt_sort_idx16_trick(__local ushort *const idx,
                           __global const float *const val,
                           const uint li,
                           const uint stride,
                           const bool parity)
{
	const bool
	active = (li % (stride << 1)) < stride;

	if(!active)
		return false;

	const uint
	oi = li + stride;

	return ircd_simt_sort_idx16_cmpxchg(idx, val, li, oi, parity);
}

/// Sort indices in `idx` which point to values contained in `val`.
inline void
ircd_simt_sort_idx16_flldr(__local ushort *const idx,
                           __global const float *const val,
                           const uint ln,
                           const uint li)
{
	for(uint up = 1; up < ln; up <<= 1)
	{
		const bool
		parity = li % (up << 2) > up;

		barrier(CLK_LOCAL_MEM_FENCE);
		ircd_simt_sort_idx16_trick(idx, val, li, up, parity);

		for(uint down = up >> 1; down > 0; down >>= 1)
		{
			barrier(CLK_LOCAL_MEM_FENCE);
			ircd_simt_sort_idx16_trick(idx, val, li, down, parity);
		}
	}
}

#endif
