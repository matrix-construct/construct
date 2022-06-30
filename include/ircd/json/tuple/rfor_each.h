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
	return util::rfor_each(t, [&f]
	(const auto &prop)
	{
		return f(prop.key, prop.value);
	});
}

template<class tuple,
         class function,
         ssize_t i = size<tuple>() - 1>
inline enable_if_tuple<tuple, bool>
rfor_each(tuple &t,
          function&& f)
{
	return util::rfor_each(t, [&f]
	(auto &prop)
	{
		return f(prop.key, prop.value);
	});
}

} // namespace json
} // namespace ircd
