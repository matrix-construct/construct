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

template<size_t i,
         class tuple,
         class function>
typename std::enable_if<i == size<tuple>(), void>::type
for_each(const tuple &t,
         function&& f)
{}

template<size_t i,
         class tuple,
         class function>
typename std::enable_if<i == size<tuple>(), void>::type
for_each(tuple &t,
         function&& f)
{}

template<size_t i = 0,
         class tuple,
         class function>
typename std::enable_if<i < size<tuple>(), void>::type
for_each(const tuple &t,
         function&& f)
{
	f(key<i>(t), val<i>(t));
	for_each<i + 1>(t, std::forward<function>(f));
}

template<size_t i = 0,
         class tuple,
         class function>
typename std::enable_if<i < size<tuple>(), void>::type
for_each(tuple &t,
         function&& f)
{
	f(key<i>(t), val<i>(t));
	for_each<i + 1>(t, std::forward<function>(f));
}

template<class tuple,
         class function>
void
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
void
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

template<class tuple,
         class function,
         ssize_t i>
typename std::enable_if<(i < 0), void>::type
rfor_each(const tuple &t,
          function&& f)
{}

template<class tuple,
         class function,
         ssize_t i>
typename std::enable_if<(i < 0), void>::type
rfor_each(tuple &t,
          function&& f)
{}

template<class tuple,
         class function,
         ssize_t i = size<tuple>() - 1>
typename std::enable_if<i < tuple_size<tuple>(), void>::type
rfor_each(const tuple &t,
          function&& f)
{
	f(key<i>(t), val<i>(t));
	rfor_each<tuple, function, i - 1>(t, std::forward<function>(f));
}

template<class tuple,
         class function,
         ssize_t i = size<tuple>() - 1>
typename std::enable_if<i < tuple_size<tuple>(), void>::type
rfor_each(tuple &t,
          function&& f)
{
	f(key<i>(t), val<i>(t));
	rfor_each<tuple, function, i - 1>(t, std::forward<function>(f));
}

} // namespace json
} // namespace ircd
