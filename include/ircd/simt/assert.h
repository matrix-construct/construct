// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2022 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_SIMT_ASSERT_H

//
// Trapping assert() on supporting platforms.
//

#if defined(__OPENCL_VERSION__)
	#if __OPENCL_VERSION__ >= 200 \
	&& !defined(assert) \
	&& !defined(NDEBUG) \
	&& __has_builtin(__builtin_trap)
		#define assert(expr)            \
		({                              \
			if(unlikely(!(bool)(x)))    \
				__builtin_trap();       \
		})
	#else
		#define assert(x)
	#endif
#endif
