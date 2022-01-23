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
#define HAVE_IRCD_SIMT_BROADCAST_H

#ifdef __OPENCL_VERSION__
/// Broadcast originating from the local leader (index [0]). All threads in the
/// group participate.
inline void
ircd_simt_broadcast_f4lldr(__local float4 *const buf,
                           const uint ln,
                           const uint li)
{
	barrier(CLK_LOCAL_MEM_FENCE);

	if(li > 0)
		buf[li] = buf[0];
}
#endif

#ifdef __OPENCL_VERSION__
/// Broadcast originating from the local leader (index [0]). All threads in the
/// group participate.
inline void
ircd_simt_broadcast_flldr(__local float *const buf,
                          const uint ln,
                          const uint li)
{
	barrier(CLK_LOCAL_MEM_FENCE);

	if(li > 0)
		buf[li] = buf[0];
}
#endif
