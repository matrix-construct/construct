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

#ifdef __OPENCL_C_VERSION__
/// Sum all elements in the buffer. All threads in the group participate;
/// result is placed in index [0], the rest of the buffer is trashed.
inline void
ircd_simt_reduce_add_f4lldr(__local float4 *const buf)
{
	const uint
	li = get_local_id(0),
	ln = get_local_size(0);

	for(uint stride = ln >> 1; stride > 0; stride >>= 1)
	{
		barrier(CLK_LOCAL_MEM_FENCE);

		if(li < stride)
			buf[li] += buf[li + stride];
	}
}
#endif

#ifdef __OPENCL_C_VERSION__
/// Sum all elements in the buffer. All threads in the group participate;
/// result is placed in index [0], the rest of the buffer is trashed.
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
