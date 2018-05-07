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
#define HAVE_IRCD_JSON_TUPLE_KEYS_H

namespace ircd {
namespace json {

template<class... T>
struct tuple<T...>::keys
:std::array<string_view, tuple<T...>::size()>
{
	struct selection;
	struct include;
	struct exclude;

	constexpr keys()
	{
		_key_transform<tuple<T...>>(this->begin(), this->end());
	}
};

template<class... T>
struct tuple<T...>::keys::selection
:std::bitset<tuple<T...>::size()>
{
	template<class closure>
	constexpr bool until(closure &&function) const
	{
		for(size_t i(0); i < this->size(); ++i)
			if(this->test(i))
				if(!function(key<tuple<T...>, i>()))
					return false;

		return true;
	}

	template<class closure>
	constexpr void for_each(closure &&function) const
	{
		this->until([&function](auto&& key)
		{
			function(key);
			return true;
		});
	}

	template<class it_a,
	         class it_b>
	constexpr auto transform(it_a it, const it_b end) const
	{
		this->until([&it, &end](auto&& key)
		{
			if(it == end)
				return false;

			*it = key;
			++it;
			return true;
		});
	}
};

template<class... T>
struct tuple<T...>::keys::include
:selection
{
	constexpr include(const std::initializer_list<string_view> &list)
	{
		for(const auto &key : list)
			this->set(indexof<tuple<T...>>(key), true);
	}
};

template<class... T>
struct tuple<T...>::keys::exclude
:selection
{
	constexpr exclude(const std::initializer_list<string_view> &list)
	{
		this->set();
		for(const auto &key : list)
			this->set(indexof<tuple<T...>>(key), false);
	}
};

} // namespace json
} // namespace ircd
