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
#define HAVE_IRCD_SIMT_REDUCE_ADD_H

#ifdef __OPENCL_VERSION__
/// Sum all elements in the buffer. All threads in the group participate;
/// result is placed in index [0], the rest of the buffer is trashed.
inline void
ircd_simt_reduce_add_f4lldr(__local float4 *const buf,
                            const uint ln,
                            const uint li)
{
	for(uint stride = ln >> 1; stride > 0; stride >>= 1)
	{
		barrier(CLK_LOCAL_MEM_FENCE);

		if(li < stride)
			buf[li] += buf[li + stride];
	}

	if(!ircd_math_is_pow2(ln) && li == 0)
		buf[li] += buf[li + 2];
}
#endif

#ifdef __OPENCL_VERSION__
/// Sum all elements in the buffer. All threads in the group participate;
/// result is placed in index [0], the rest of the buffer is trashed.
inline void
ircd_simt_reduce_add_flldr(__local float *const buf,
                           const uint ln,
                           const uint li)
#if defined(cl_khr_subgroups)
{
	const float
	ret = work_group_reduce_add(buf[li]);

	if(li == 0)
		buf[li] = ret;
}
#else
{
	for(uint stride = ln >> 1; stride > 0; stride >>= 1)
	{
		barrier(CLK_LOCAL_MEM_FENCE);

		if(li < stride)
			buf[li] += buf[li + stride];
	}

	if(!ircd_math_is_pow2(ln) && li == 0)
		buf[li] += buf[li + 2];
}
#endif
#endif

#ifdef __OPENCL_VERSION__
/// Sum all elements in the buffer. All threads in the group participate;
/// result is placed in index [0], the rest of the buffer is trashed.
inline void
ircd_simt_reduce_add_illdr(__local int *const buf,
                           const uint ln,
                           const uint li)
{
	if(li > 0)
		atomic_add(buf + 0, buf[li]);
}
#endif

#ifdef __OPENCL_VERSION__
/// Sum all elements in the buffer. All threads in the group participate;
/// result is placed in index [0], the rest of the buffer is trashed.
inline void
ircd_simt_reduce_add_ulldr(__local uint *const buf,
                           const uint ln,
                           const uint li)
{
	if(li > 0)
		atomic_add(buf + 0, buf[li]);
}
#endif

#ifdef __OPENCL_VERSION__
inline float
__attribute__((always_inline))
ircd_simt_reduce_add_f4(const float4 in)
{
	float ret = 0.0f;
	for(uint i = 0; i < 4; ++i)
		ret += in[i];

	return ret;
}
#endif
