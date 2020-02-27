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
#define HAVE_IRCD_UTIL_IDENTITY_H

namespace ircd
{
	inline namespace util
	{
		struct identity;
	}
}

struct ircd::util::identity
{
	template<class T>
	constexpr T&& operator()(T&& t) const noexcept;
};

template<class T>
constexpr inline T&&
ircd::util::identity::operator()(T&& t)
const noexcept
{
	return std::forward<T>(t);
}
