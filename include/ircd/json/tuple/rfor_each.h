// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2022 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_JSON_TUPLE_RFOR_EACH_H

namespace ircd {
namespace json {

template<class tuple,
         class function,
         ssize_t i = size<tuple>() - 1>
inline enable_if_tuple<tuple, bool>
rfor_each(const tuple &t,
          function&& f)
{
	if constexpr(i >= 0)
	{
		using closure_result = std::invoke_result_t
		<
			decltype(f), decltype(key<i>(t)), decltype(val<i>(t))
		>;

		constexpr bool terminable
		{
			std::is_same<closure_result, bool>()
		};

		if constexpr(terminable)
		{
			if(!f(key<i>(t), val<i>(t)))
				return false;
		}
		else f(key<i>(t), val<i>(t));

		return rfor_each<tuple, function, i - 1>(t, std::forward<function>(f));
	}
	else return true;
}

template<class tuple,
         class function,
         ssize_t i = size<tuple>() - 1>
inline enable_if_tuple<tuple, bool>
rfor_each(tuple &t,
          function&& f)
{
	if constexpr(i >= 0)
	{
		using closure_result = std::invoke_result_t
		<
			decltype(f), decltype(key<i>(t)), decltype(val<i>(t))
		>;

		constexpr bool terminable
		{
			std::is_same<closure_result, bool>()
		};

		if constexpr(terminable)
		{
			if(!f(key<i>(t), val<i>(t)))
				return false;
		}
		else f(key<i>(t), val<i>(t));

		return rfor_each<tuple, function, i - 1>(t, std::forward<function>(f));
	}
	else return true;
}

} // namespace json
} // namespace ircd
