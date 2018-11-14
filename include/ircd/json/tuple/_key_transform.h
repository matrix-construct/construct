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
#define HAVE_IRCD_JSON_TUPLE__KEY_TRANSFORM_H

namespace ircd {
namespace json {

template<class tuple,
         class it_a,
         class it_b,
         size_t i,
         class closure>
constexpr typename std::enable_if<i == tuple::size(), it_a>::type
_key_transform(it_a it,
               const it_b &end,
               closure&& lambda)
{
	return it;
}

template<class tuple,
         class it_a,
         class it_b,
         size_t i = 0,
         class closure>
constexpr typename std::enable_if<i < tuple::size(), it_a>::type
_key_transform(it_a it,
               const it_b &end,
               closure&& lambda)
{
	if(it != end)
	{
		*it = lambda(key<tuple, i>());
		++it;
	}

	return _key_transform<tuple, it_a, it_b, i + 1>(it, end, std::move(lambda));
}

template<class tuple,
         class it_a,
         class it_b>
constexpr auto
_key_transform(it_a it,
               const it_b &end)
{
	return _key_transform<tuple>(it, end, []
	(auto&& key)
	{
		return key;
	});
}

template<class it_a,
         class it_b,
         class... T>
auto
_key_transform(const tuple<T...> &tuple,
               it_a it,
               const it_b &end)
{
	for_each(tuple, [&it, &end]
	(const auto &key, const auto &val)
	{
		if(it != end)
		{
			*it = key;
			++it;
		}
	});

	return it;
}

} // namespace json
} // namespace ircd
