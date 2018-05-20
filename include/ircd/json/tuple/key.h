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
constexpr enable_if_tuple<tuple, const char *const &>
key()
{
	return tuple_element<tuple, i>::key;
}

template<class tuple,
         size_t i>
constexpr typename std::enable_if<i == tuple::size(), const char *>::type
key(const size_t &j)
{
	return nullptr;
}

template<class tuple,
         size_t i = 0>
constexpr typename std::enable_if<i < tuple::size(), const char *>::type
key(const size_t &j)
{
	return j == i?
		tuple_element<tuple, i>::key:
		key<tuple, i + 1>(j);
}

template<size_t i,
         class tuple>
enable_if_tuple<tuple, const char *const &>
key(const tuple &t)
{
	return std::get<i>(t).key;
}

} // namespace json
} // namespace ircd
