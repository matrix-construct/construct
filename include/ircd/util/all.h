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
#define HAVE_IRCD_UTIL_ALL_H

namespace ircd {
inline namespace util
{
	template<size_t SIZE,
	         class T>
	bool all(const T (&vec)[SIZE]) noexcept;

	template<size_t SIZE>
	bool all(const bool (&vec)[SIZE]) noexcept;
}}

template<size_t SIZE>
inline bool
ircd::util::all(const bool (&vec)[SIZE])
noexcept
{
	return std::all_of(vec, vec + SIZE, identity{});
}
