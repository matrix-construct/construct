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
         size_t i>
constexpr typename std::enable_if<i == size<tuple>(), size_t>::type
indexof()
{
	return size<tuple>();
}

template<class tuple,
         size_t hash,
         size_t i = 0>
constexpr typename std::enable_if<i < size<tuple>(), size_t>::type
indexof()
{
	constexpr auto equal
	{
		ircd::hash(key<tuple, i>()) == hash
	};

	return equal? i : indexof<tuple, hash, i + 1>();
}

template<class tuple,
         const char *const &name,
         size_t i>
constexpr typename std::enable_if<i == size<tuple>(), size_t>::type
indexof()
{
	return size<tuple>();
}

template<class tuple,
         const char *const &name,
         size_t i = 0>
constexpr typename std::enable_if<i < size<tuple>(), size_t>::type
indexof()
{
	return indexof<tuple, name_hash(name)>();
}

template<class tuple,
         size_t i>
constexpr typename std::enable_if<i == size<tuple>(), size_t>::type
indexof(const char *const &name)
{
	return size<tuple>();
}

template<class tuple,
         size_t i = 0>
constexpr typename std::enable_if<i < size<tuple>(), size_t>::type
indexof(const char *const &name)
{
	constexpr auto equal
	{
		_constexpr_equal(key<tuple, i>(), name)
	};

	return equal? i : indexof<tuple, i + 1>(name);
}

template<class tuple,
         size_t i>
constexpr typename std::enable_if<i == size<tuple>(), size_t>::type
indexof(const string_view &name)
{
	return size<tuple>();
}

template<class tuple,
         size_t i = 0>
constexpr typename std::enable_if<i < size<tuple>(), size_t>::type
indexof(const string_view &name)
{
	const auto equal
	{
		name == key<tuple, i>()
	};

	return equal? i : indexof<tuple, i + 1>(name);
}

} // namespace json
} // namespace ircd
