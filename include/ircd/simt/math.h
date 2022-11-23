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
#define HAVE_IRCD_SIMT_MATH_H

#ifdef __OPENCL_VERSION__
inline bool
ircd_math_is_pow2(const uint val)
{
	return val > 0 && (val & (val - 1)) == 0;
}
#endif
