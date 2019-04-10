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
#define HAVE_IRCD_UTIL_CLOSURE_H

namespace ircd::util
{
	template<class function> struct closure;
}

///TODO: This is a WIP that is meant to replace the pattern of having two
///TODO: for_each() overloads in every interface where one takes a closure
///TODO: which returns a bool, and the other takes a void closure. In practice
///TODO: the void overload simply calls the bool overload and returns true.
template<class function>
struct ircd::util::closure
{

};
