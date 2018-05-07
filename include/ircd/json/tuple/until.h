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
#define HAVE_IRCD_JSON_TUPLE_UNTIL_H

namespace ircd {
namespace json {

template<size_t i,
         class tuple,
         class function>
typename std::enable_if<i == size<tuple>(), bool>::type
until(const tuple &t,
      function&& f)
{
	return true;
}

template<size_t i,
         class tuple,
         class function>
typename std::enable_if<i == size<tuple>(), bool>::type
until(tuple &t,
      function&& f)
{
	return true;
}

template<size_t i = 0,
         class tuple,
         class function>
typename std::enable_if<i < size<tuple>(), bool>::type
until(const tuple &t,
      function&& f)
{
	return f(key<i>(t), val<i>(t))?
	       until<i + 1>(t, std::forward<function>(f)):
	       false;
}

template<size_t i = 0,
         class tuple,
         class function>
typename std::enable_if<i < size<tuple>(), bool>::type
until(tuple &t,
      function&& f)
{
	return f(key<i>(t), val<i>(t))?
	       until<i + 1>(t, std::forward<function>(f)):
	       false;
}

template<size_t i,
         class tuple,
         class function>
typename std::enable_if<i == size<tuple>(), bool>::type
until(const tuple &a,
      const tuple &b,
      function&& f)
{
	return true;
}

template<size_t i = 0,
         class tuple,
         class function>
typename std::enable_if<i < size<tuple>(), bool>::type
until(const tuple &a,
      const tuple &b,
      function&& f)
{
	return f(key<i>(a), val<i>(a), val<i>(b))?
	       until<i + 1>(a, b, std::forward<function>(f)):
	       false;
}

template<class tuple,
         class function,
         ssize_t i>
typename std::enable_if<(i < 0), bool>::type
runtil(const tuple &t,
       function&& f)
{
	return true;
}

template<class tuple,
         class function,
         ssize_t i>
typename std::enable_if<(i < 0), bool>::type
runtil(tuple &t,
       function&& f)
{
	return true;
}

template<class tuple,
         class function,
         ssize_t i = size<tuple>() - 1>
typename std::enable_if<i < size<tuple>(), bool>::type
runtil(const tuple &t,
       function&& f)
{
	return f(key<i>(t), val<i>(t))?
	       runtil<tuple, function, i - 1>(t, std::forward<function>(f)):
	       false;
}

template<class tuple,
         class function,
         ssize_t i = size<tuple>() - 1>
typename std::enable_if<i < size<tuple>(), bool>::type
runtil(tuple &t,
       function&& f)
{
	return f(key<i>(t), val<i>(t))?
	       runtil<tuple, function, i - 1>(t, std::forward<function>(f)):
	       false;
}

} // namespace json
} // namespace ircd
