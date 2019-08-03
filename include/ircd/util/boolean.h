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
#define HAVE_IRCD_UTIL_BOOLEAN_H

namespace ircd
{
	inline namespace util
	{
		struct boolean;
	}
}

/// Simple convenience for cheesers to inherit from a POD boolean, similar to
/// IRCD_STRONG_TYPEDEF(bool, boolean) but with additional specific features.
struct ircd::util::boolean
{
	bool val;

	operator const bool &() const noexcept
	{
		return val;
	}

	explicit operator bool &() & noexcept
	{
		return val;
	}

	boolean(const bool &val) noexcept
	:val{val}
	{}

	boolean(const std::function<bool ()> &func)
	:val{func()}
	{}
};
