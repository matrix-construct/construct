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
#define HAVE_IRCD_JSON_TUPLE_INDEXOF_H

namespace ircd {
namespace json {

template<class tuple,
         size_t hash,
         size_t i = 0>
constexpr enable_if_tuple<tuple, size_t>
indexof()
noexcept
{
	if constexpr(i < size<tuple>())
		if constexpr(name_hash(key<tuple, i>()) != hash)
			return indexof<tuple, hash, i + 1>();

	return i;
}

template<class tuple,
         const char *const &name,
         size_t i = 0>
constexpr enable_if_tuple<tuple, size_t>
indexof()
noexcept
{
	return indexof<tuple, name_hash(name)>();
}

template<class tuple,
         size_t i = 0,
         size_t N>
inline enable_if_tuple<tuple, size_t>
indexof(const char (&name)[N])
noexcept
{
	if constexpr(i < size<tuple>())
		if(!_constexpr_equal(key<tuple, i>(), name))
			return indexof<tuple, i + 1>(name);

	return i;
}

template<class tuple,
         size_t i = 0>
inline enable_if_tuple<tuple, size_t>
indexof(const string_view &name)
noexcept
{
	if constexpr(i < size<tuple>())
	{
		constexpr string_view key_name
		{
			key<tuple, i>()
		};

		if(!_constexpr_equal(name, key_name))
			return indexof<tuple, i + 1>(name);
	}

	return i;
}

} // namespace json
} // namespace ircd
