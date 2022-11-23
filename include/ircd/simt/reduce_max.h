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
#define HAVE_IRCD_SIMT_REDUCE_MAX_H

#ifdef __OPENCL_VERSION__
/// Find the greatest value in the buffer. All threads in the group participate;
/// the greatest value is placed in index [0], the rest of the buffer is
/// trashed.
inline void
ircd_simt_reduce_max_flldr(__local float *const buf,
                           const uint ln,
                           const uint li)
#if defined(cl_khr_subgroups)
{
	const float
	ret = work_group_reduce_max(buf[li]);

	if(li == 0)
		buf[li] = ret;
}
#else
{
	for(uint stride = ln >> 1; stride > 0; stride >>= 1)
	{
		barrier(CLK_LOCAL_MEM_FENCE);

		if(li < stride)
			if(buf[li] < buf[li + stride])
				buf[li] = buf[li + stride];
	}

	if(!ircd_math_is_pow2(ln) && li == 0)
		if(buf[li] < buf[li + 2])
			buf[li] = buf[li + 2];
}
#endif
#endif

#ifdef __OPENCL_VERSION__
inline void
ircd_simt_reduce_max_illdr(__local int *const buf,
                           const uint ln,
                           const uint li)
{
	buf[li] = atomic_max(buf, buf[li]);
}
#endif

#ifdef __OPENCL_VERSION__
inline void
ircd_simt_reduce_max_ulldr(__local uint *const buf,
                           const uint ln,
                           const uint li)
{
	buf[li] = atomic_max(buf, buf[li]);
}
#endif
