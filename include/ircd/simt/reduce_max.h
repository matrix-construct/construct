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

#ifdef __OPENCL_C_VERSION__
/// Find the greatest value in the buffer. All threads in the group participate;
/// the greatest value is placed in index [0], the rest of the buffer is
/// trashed.
inline void
ircd_simt_reduce_max_flldr(__local float *const buf)
{
	const uint
	li = get_local_id(0),
	ln = get_local_size(0);

	for(uint stride = ln >> 1; stride > 0; stride >>= 1)
	{
		barrier(CLK_LOCAL_MEM_FENCE);

		if(li < stride)
			if(buf[li] < buf[li + stride])
				buf[li] = buf[li + stride];
	}
}
#endif
