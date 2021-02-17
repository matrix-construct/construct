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
#define HAVE_IRCD_MATH_LOG2_H

namespace ircd::math
{
	template<class Z> constexpr bool is_pow2(const Z);
	template<class Z> constexpr Z next_pow2(const Z);
	template<class Z> constexpr Z log2(const Z);
	template<class Z> constexpr Z sqr(const Z);
}

template<class Z>
inline constexpr Z
ircd::math::sqr(const Z n)
{
	return pow(n, 2);
}

template<class Z>
inline constexpr Z
ircd::math::log2(const Z n)
{
	return n > Z{1}?
		Z{1} + log2(n >> 1):
		0;
}

template<class Z>
inline constexpr Z
ircd::math::next_pow2(Z v)
{
	--v;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	v |= v >> 32;
	return ++v;
}

template<class Z>
inline constexpr bool
ircd::math::is_pow2(const Z v)
{
	return v && !(v & (v - Z{1}));
}
