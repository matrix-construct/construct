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
#define HAVE_IRCD_JSON_TUPLE_KEY_H

namespace ircd {
namespace json {

template<class tuple,
         size_t i>
constexpr enable_if_tuple<tuple, const char *>
key()
noexcept
{
	return tuple_element<tuple, i>::key;
}

template<size_t i,
         class tuple>
inline enable_if_tuple<tuple, const char *>
key(const tuple &t)
noexcept
{
	return std::get<i>(t).key;
}

template<class tuple,
         size_t i>
inline typename std::enable_if<i == tuple::size(), const char *>::type
key(const size_t &j)
noexcept
{
	return nullptr;
}

template<class tuple,
         size_t i = 0>
inline typename std::enable_if<i < tuple::size(), const char *>::type
key(const size_t &j)
noexcept
{
	if(likely(j != i))
		return key<tuple, i + 1>(j);

	return tuple_element<tuple, i>::key;
}

} // namespace json
} // namespace ircd
