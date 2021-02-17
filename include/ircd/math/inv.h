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
#define HAVE_IRCD_MATH_INV_H

namespace ircd::math
{
	template<class Z> constexpr Z inv(Z a, const Z m);
}

/// Modular Inverse
template<class Z>
inline constexpr Z
ircd::math::inv(Z a,
                const Z m)
{
	auto b{m}, x0{0}, x1{1};
	if(likely(m != 1)) while(a > 1)
	{
		auto t{b}, q{a / b};
		b = a % b;
		a = t;
		t = x0;
		x0 = x1 - q * x0;
		x1 = t;
	}

	x1 += boolmask<Z>(x1 < 0) & m;
	return x1;
}
