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
#define HAVE_IRCD_UTIL_COMPARE_EXCHANGE_H

namespace ircd {
inline namespace util
{
	template<class T>
	bool compare_exchange(T &value, T &expect, T desire) noexcept;
}}

/// Non-atomic compare_exchange().
template<class T>
inline bool
ircd::util::compare_exchange(T &value,
                             T &expect,
                             T desire)
noexcept
{
	const bool ret
	{
		value == expect
	};

	value = ret? desire: value;
	expect = ret? expect: value;
	return ret;
}
