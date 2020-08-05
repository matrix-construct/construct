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
#define HAVE_IRCD_JSON_TUPLE_AT_H

namespace ircd {
namespace json {

template<size_t hash,
         class tuple>
inline enable_if_tuple<tuple, const tuple_value_type<tuple, indexof<tuple, hash>()> &>
at(const tuple &t)
{
	constexpr size_t idx
	{
		indexof<tuple, hash>()
	};

	const auto &ret
	{
		val<idx>(t)
	};

	if(unlikely(!defined(json::value(ret))))
		throw not_found
		{
			"%s", key<idx>(t)
		};

	return ret;
}

template<size_t hash,
         class tuple>
inline enable_if_tuple<tuple, tuple_value_type<tuple, indexof<tuple, hash>()> &>
at(tuple &t)
{
	constexpr size_t idx
	{
		indexof<tuple, hash>()
	};

	auto &ret
	{
		val<idx>(t)
	};

	if(unlikely(!defined(json::value(ret))))
		throw not_found
		{
			"%s", key<idx>(t)
		};

	return ret;
}

template<const char *const &name,
         class tuple>
inline enable_if_tuple<tuple, const tuple_value_type<tuple, indexof<tuple, name>()> &>
at(const tuple &t)
{
	return at<name_hash(name), tuple>(t);
}

template<const char *const &name,
         class tuple>
inline enable_if_tuple<tuple, tuple_value_type<tuple, indexof<tuple, name>()> &>
at(tuple &t)
{
	return at<name_hash(name), tuple>(t);
}

template<class tuple,
         class function,
         size_t i>
constexpr typename std::enable_if<i == size<tuple>(), void>::type
at(tuple &t,
   const string_view &name,
   function&& f)
noexcept
{}

template<class tuple,
         class function,
         size_t i = 0>
inline typename std::enable_if<i < size<tuple>(), void>::type
at(tuple &t,
   const string_view &name,
   function&& f)
{
	if(indexof<tuple>(name) == i)
		f(val<i>(t));
	else
		at<tuple, function, i + 1>(t, name, std::forward<function>(f));
}

template<class tuple,
         class function,
         size_t i>
constexpr typename std::enable_if<i == size<tuple>(), void>::type
at(const tuple &t,
   const string_view &name,
   function&& f)
noexcept
{}

template<class tuple,
         class function,
         size_t i = 0>
inline typename std::enable_if<i < size<tuple>(), void>::type
at(const tuple &t,
   const string_view &name,
   function&& f)
{
	if(indexof<tuple>(name) == i)
		f(val<i>(t));
	else
		at<tuple, function, i + 1>(t, name, std::forward<function>(f));
}

template<class R,
         class tuple>
inline enable_if_tuple<tuple, const R &>
at(const tuple &t,
   const string_view &name)
{
	const R *ret;
	const auto closure
	{
		[&name, &ret](const auto &key, const auto &val) noexcept
		{
			if constexpr(std::is_assignable<R, decltype(val)>())
			{
				if(key == name)
				{
					ret = std::addressof(val);
					return false;
				}
				else return true;
			}
			else return true;
		}
	};

	if(unlikely(until(t, closure)))
		throw not_found
		{
			"%s", name
		};

	return *ret;
}

template<class R,
         class tuple>
inline enable_if_tuple<tuple, R &>
at(tuple &t,
   const string_view &name)
{
	R *ret;
	const auto closure
	{
		[&name, &ret](const auto &key, auto &val) noexcept
		{
			if constexpr(std::is_assignable<R, decltype(val)>())
			{
				if(key == name)
				{
					ret = std::addressof(val);
					return false;
				}
				else return true;
			}
			else return true;
		}
	};

	if(unlikely(until(t, closure)))
		throw not_found
		{
			"%s", name
		};

	return *ret;
}

} // namespace json
} // namespace ircd
