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
#define HAVE_IRCD_CAST_H

namespace ircd::simd
{
	template<class R,
	         class T>
	const R *cast(const T *const &) noexcept;

	template<class R,
	         class T>
	R *cast(T *const &) noexcept;
}

template<class R,
         class T>
inline R *
ircd::simd::cast(T *const &in)
noexcept
{
	assert(uintptr_t(in) % std::alignment_of<R>::value == 0);
	return reinterpret_cast<R *>(in);
}

template<class R,
         class T>
inline const R *
ircd::simd::cast(const T *const &in)
noexcept
{
	assert(uintptr_t(in) % std::alignment_of<R>::value == 0);
	return reinterpret_cast<const R *>(in);
}
