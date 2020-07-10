// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_SIMD_LANE_CAST_H

namespace ircd::simd
{
	template<class R,
	         class T>
	R lane_cast(const T &);
}

/// Convert each lane from a smaller type to a larger type
template<class R,
         class T>
inline R
ircd::simd::lane_cast(const T &in)
{
	static_assert
	(
		lanes<R>() == lanes<T>(), "Types must have matching number of lanes."
	);

	#if __has_builtin(__builtin_convertvector)
		return __builtin_convertvector(in, R);
	#endif

	R ret;
	for(size_t i(0); i < lanes<R>(); ++i)
		ret[i] = in[i];

	return ret;
}
