// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2023 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_UTIL_RETURNS_H

namespace ircd
{
	inline namespace util
	{
		template<class T>
		struct returns;
	}
}

/// Simple convenience for cheesers to inherit from a POD type, similar to
/// IRCD_STRONG_TYPEDEF but with additional specific features.
template<class T>
struct ircd::util::returns
{
	T ret;

	operator const T &() const noexcept
	{
		return ret;
	}

	explicit operator T &() & noexcept
	{
		return ret;
	}

	returns(const std::function<T ()> &func)
	:ret{func()}
	{}

	returns(const T &ret) noexcept
	:ret{ret}
	{}

	returns() = default;
};
