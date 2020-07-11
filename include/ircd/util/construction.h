// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_UTIL_CONSTRUCTION_H

namespace ircd
{
	inline namespace util
	{
		struct construction;
	}
}

struct ircd::util::construction
{
	template<class F,
	         class... A>
	construction(F&&, A&&...);
};

template<class F,
         class... A>
[[using gnu: always_inline, artificial]]
inline
ircd::util::construction::construction(F&& f,
                                       A&&... a)
{
	f(std::forward<A>(a)...);
}
