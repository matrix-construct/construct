// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_JSON_TUPLE_TOOL_H

namespace ircd {
namespace json {

template<class... T>
size_t
serialized(const tuple<T...> &t)
{
	constexpr const size_t member_count
	{
		tuple<T...>::size()
	};

	std::array<size_t, member_count> sizes {0};
	const auto e{_member_transform_if(t, begin(sizes), end(sizes), []
	(auto &ret, const string_view &key, auto&& val)
	{
		const json::value value(val);
		if(!defined(value))
			return false;

		ret = 1 + key.size() + 1 + 1 + serialized(value) + 1;
		return true;
	})};

	// Subtract one to get the final size when an extra comma is
	// accumulated on non-empty objects.
	const auto overhead
	{
		1 + std::all_of(begin(sizes), e, is_zero{})
	};

	return std::accumulate(begin(sizes), e, size_t(overhead));
}

template<class... T>
size_t
serialized(const tuple<T...> *const &b,
           const tuple<T...> *const &e)
{
	size_t ret(1 + (b == e));
	return std::accumulate(b, e, ret, []
	(size_t ret, const tuple<T...> &t)
	{
		return ret += serialized(t) + 1;
	});
}

template<class... T>
string_view
stringify(mutable_buffer &buf,
          const tuple<T...> &tuple)
{
	static constexpr const size_t member_count
	{
		json::tuple_size<json::tuple<T...>>()
	};

	std::array<member, member_count> members;
	const auto e{_member_transform_if(tuple, begin(members), end(members), []
	(auto &ret, const string_view &key, auto&& val)
	{
		json::value value(val);
		if(!defined(value))
			return false;

		ret = member { key, std::move(value) };
		return true;
	})};

	return stringify(buf, begin(members), e);
}

template<class... T>
string_view
stringify(mutable_buffer &buf,
          const tuple<T...> *b,
          const tuple<T...> *e)
{
	const auto start(begin(buf));
	consume(buf, copy(buf, '['));
	if(b != e)
	{
		stringify(buf, *b);
		for(++b; b != e; ++b)
		{
			consume(buf, copy(buf, ','));
			stringify(buf, *b);
		}
	}
	consume(buf, copy(buf, ']'));
	return { start, begin(buf) };
}

template<class... T>
std::ostream &
operator<<(std::ostream &s, const tuple<T...> &t)
{
    s << json::strung(t);
    return s;
}

} // namespace json
} // namespace ircd
