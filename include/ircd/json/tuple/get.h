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
#define HAVE_IRCD_JSON_TUPLE_GET_H

namespace ircd {
namespace json {

template<size_t hash,
         class tuple>
enable_if_tuple<tuple, const tuple_value_type<tuple, indexof<tuple, hash>()> &>
get(const tuple &t)
{
	constexpr size_t idx
	{
		indexof<tuple, hash>()
	};

	const auto &ret
	{
		val<idx>(t)
	};

	return ret;
}

template<size_t hash,
         class tuple>
enable_if_tuple<tuple, tuple_value_type<tuple, indexof<tuple, hash>()>>
get(const tuple &t,
    const tuple_value_type<tuple, indexof<tuple, hash>()> &def)
{
	constexpr size_t idx
	{
		indexof<tuple, hash>()
	};

	const auto &ret
	{
		val<idx>(t)
	};

	return defined(json::value(ret))? ret : def;
}

template<size_t hash,
         class tuple>
enable_if_tuple<tuple, tuple_value_type<tuple, indexof<tuple, hash>()> &>
get(tuple &t)
{
	constexpr size_t idx
	{
		indexof<tuple, hash>()
	};

	auto &ret
	{
		val<idx>(t)
	};

	return ret;
}

template<size_t hash,
         class tuple>
enable_if_tuple<tuple, tuple_value_type<tuple, indexof<tuple, hash>()> &>
get(tuple &t,
    tuple_value_type<tuple, indexof<tuple, hash>()> &def)
{
	auto &ret
	{
		get<hash, tuple>(t)
	};

	return defined(json::value(ret))? ret : def;
}

template<const char *const &name,
         class tuple>
enable_if_tuple<tuple, const tuple_value_type<tuple, indexof<tuple, name>()> &>
get(const tuple &t)
{
	return get<name_hash(name), tuple>(t);
}

template<const char *const &name,
         class tuple>
enable_if_tuple<tuple, tuple_value_type<tuple, indexof<tuple, name>()>>
get(const tuple &t,
    const tuple_value_type<tuple, indexof<tuple, name>()> &def)
{
	return get<name_hash(name), tuple>(t, def);
}

template<const char *const &name,
         class tuple>
enable_if_tuple<tuple, tuple_value_type<tuple, indexof<tuple, name>()>>
get(tuple &t)
{
	return get<name_hash(name), tuple>(t);
}

template<const char *const &name,
         class tuple>
enable_if_tuple<tuple, tuple_value_type<tuple, indexof<tuple, name>()>>
get(tuple &t,
    tuple_value_type<tuple, indexof<tuple, hash>()> &def)
{
	return get<name_hash(name), tuple>(t, def);
}

} // namespace json
} // namespace ircd
