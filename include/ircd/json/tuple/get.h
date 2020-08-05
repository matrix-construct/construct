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
inline enable_if_tuple<tuple, const tuple_value_type<tuple, indexof<tuple, hash>()> &>
get(const tuple &t)
noexcept
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
inline enable_if_tuple<tuple, tuple_value_type<tuple, indexof<tuple, hash>()>>
get(const tuple &t,
    const tuple_value_type<tuple, indexof<tuple, hash>()> &def)
noexcept
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
inline enable_if_tuple<tuple, tuple_value_type<tuple, indexof<tuple, hash>()> &>
get(tuple &t)
noexcept
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
inline enable_if_tuple<tuple, tuple_value_type<tuple, indexof<tuple, hash>()> &>
get(tuple &t,
    tuple_value_type<tuple, indexof<tuple, hash>()> &def)
noexcept
{
	auto &ret
	{
		get<hash, tuple>(t)
	};

	return defined(json::value(ret))? ret : def;
}

template<const char *const &name,
         class tuple>
inline enable_if_tuple<tuple, const tuple_value_type<tuple, indexof<tuple, name>()> &>
get(const tuple &t)
noexcept
{
	return get<name_hash(name), tuple>(t);
}

template<const char *const &name,
         class tuple>
inline enable_if_tuple<tuple, tuple_value_type<tuple, indexof<tuple, name>()>>
get(const tuple &t,
    const tuple_value_type<tuple, indexof<tuple, name>()> &def)
noexcept
{
	return get<name_hash(name), tuple>(t, def);
}

template<const char *const &name,
         class tuple>
inline enable_if_tuple<tuple, tuple_value_type<tuple, indexof<tuple, name>()>>
get(tuple &t)
noexcept
{
	return get<name_hash(name), tuple>(t);
}

template<const char *const &name,
         class tuple>
inline enable_if_tuple<tuple, tuple_value_type<tuple, indexof<tuple, name>()>>
get(tuple &t,
    tuple_value_type<tuple, indexof<tuple, hash>()> &def)
noexcept
{
	return get<name_hash(name), tuple>(t, def);
}

template<class R,
         class tuple>
inline enable_if_tuple<tuple, R>
get(const tuple &t,
    const string_view &name,
    R ret)
noexcept
{
	until(t, [&name, &ret]
	(const auto &key, auto&& val)
	noexcept
	{
		if constexpr(std::is_assignable<R, decltype(val)>())
		{
			if(key == name)
			{
				ret = val;
				return false;
			}
			else return true;
		}
		else return true;
	});

	return ret;
}

} // namespace json
} // namespace ircd
