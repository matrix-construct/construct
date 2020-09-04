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
#define HAVE_IRCD_SIMD_LOWER_H

namespace ircd::simd
{
	template<class R,
	         class T>
	R lower(const T) noexcept;
}

template<class R,
         class T>
inline R
ircd::simd::lower(const T a)
noexcept
{
	static_assert
	(
		sizeof(R) * 2 == sizeof(T),
		"Types must have equal size."
	);

	static_assert
	(
		lanes<R>() * 2 == lanes<T>(),
		"T must have twice as many lanes as R"
	);

	R ret;
	for(size_t i(0); i < lanes<R>(); ++i)
		ret[i] = a[i];

	return ret;
}
