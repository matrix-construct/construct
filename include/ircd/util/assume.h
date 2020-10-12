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
#define HAVE_IRCD_UTIL_ASSUME_H

namespace ircd { inline namespace util
{
	template<class T>
	[[using gnu: always_inline, gnu_inline, artificial]]
	extern inline void
	assume(T&& expr)
	{
		assert(static_cast<T&&>(expr));
		#if __has_builtin(__builtin_assume)
			__builtin_assume(static_cast<T&&>(expr));
		#endif
	}
}}
