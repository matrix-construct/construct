// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_UTIL_REENTRANCE_H

namespace ircd {
inline namespace util
{
	template<bool &entered> struct reentrance_assertion;
}}

/// Simple assert for reentrancy; useful when static variables are in play.
/// You have to place `entered` and give it the proper linkage you want.
template<bool &entered>
struct ircd::util::reentrance_assertion
{
	reentrance_assertion()
	{
		assert(!entered);
		entered = true;
	}

	~reentrance_assertion()
	{
		assert(entered);
		entered = false;
	}
};
