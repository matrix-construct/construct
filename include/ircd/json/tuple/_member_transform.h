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
#define HAVE_IRCD_JSON_TUPLE__MEMBER_TRANSFORM_H

namespace ircd {
namespace json {

template<class it_a,
         class it_b,
         class closure,
         class... T>
auto
_member_transform_if(const tuple<T...> &tuple,
                     it_a it,
                     const it_b end,
                     closure&& lambda)
{
	until(tuple, [&it, &end, &lambda]
	(const auto &key, auto&& val)
	{
		if(it == end)
			return false;

		if(lambda(*it, key, val))
			++it;

		return true;
	});

	return it;
}

template<class it_a,
         class it_b,
         class closure,
         class... T>
auto
_member_transform(const tuple<T...> &tuple,
                  it_a it,
                  const it_b end,
                  closure&& lambda)
{
	return _member_transform_if(tuple, it, end, [&lambda]
	(auto&& ret, const auto &key, auto&& val)
	{
		ret = lambda(key, val);
		return true;
	});
}

template<class it_a,
         class it_b,
         class... T>
auto
_member_transform(const tuple<T...> &tuple,
                  it_a it,
                  const it_b end)
{
	return _member_transform(tuple, it, end, []
	(auto&& ret, const auto &key, auto&& val) -> member
	{
		return { key, val };
	});
}

} // namespace json
} // namespace ircd
