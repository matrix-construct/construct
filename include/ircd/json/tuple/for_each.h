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
#define HAVE_IRCD_JSON_TUPLE_FOR_EACH_H

namespace ircd {
namespace json {

template<class tuple,
         class function>
inline enable_if_tuple<tuple, bool>
for_each(const tuple &t,
         function&& f)
{
	return util::for_each(t, [&f]
	(const auto &prop)
	{
		return f(prop.key, prop.value);
	});
}

template<class tuple,
         class function>
inline enable_if_tuple<tuple, bool>
for_each(tuple &t,
         function&& f)
{
	return util::for_each(t, [&f]
	(auto &prop)
	{
		return f(prop.key, prop.value);
	});
}

template<class tuple,
         class function>
inline void
for_each(const tuple &t,
         const vector_view<const string_view> &mask,
         function&& f)
{
	std::for_each(std::begin(mask), std::end(mask), [&t, &f]
	(const auto &key)
	{
		at(t, key, [&f, &key]
		(auto&& val)
		{
			f(key, val);
		});
	});
}

template<class tuple,
         class function>
inline void
for_each(tuple &t,
         const vector_view<const string_view> &mask,
         function&& f)
{
	std::for_each(std::begin(mask), std::end(mask), [&t, &f]
	(const auto &key)
	{
		at(t, key, [&f, &key]
		(auto&& val)
		{
			f(key, val);
		});
	});
}

} // namespace json
} // namespace ircd
