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

template<class tuple> struct keys;

} // namespace json
} // namespace ircd

/// Array of string literals (in string_views) representing just the keys of a
/// tuple. By default construction all keys are included in the array. A
/// selection construction will only include keys that are selected. Note that
/// the selection construction packs all the chosen keys at the front of the
/// array so you cannot rely on this for a key's index into the tuple.
template<class tuple>
struct ircd::json::keys
:std::array<string_view, tuple::size()>
{
	struct selection;
	struct include;
	struct exclude;

	size_t count() const;

	operator vector_view<const string_view>() const;

	constexpr keys(const selection & = {});
};

/// Selection of keys in a tuple represented by a bitset. Users generally
/// do not construct this class directly. Instead, construct one of the
/// `include` or `exclude` classes which will set these bits appropriately.
template<class tuple>
struct ircd::json::keys<tuple>::selection
:std::bitset<tuple::size()>
{
	template<class closure>
	constexpr bool until(closure &&function) const;

	template<class closure>
	constexpr void for_each(closure &&function) const;

	template<class it_a,
	         class it_b>
	constexpr it_a transform(it_a it, const it_b end) const;

	constexpr selection(const uint64_t &val = -1)
	:std::bitset<tuple::size()>{val}
	{}

	static_assert(tuple::size() <= sizeof(uint64_t) * 8);
};

/// Construct this class with a list of keys you want to select for a given
/// tuple. This constructs a bitset representing the keys of the tuple and
/// lights the bits for your selections.
template<class tuple>
struct ircd::json::keys<tuple>::include
:selection
{
	include(const vector_view<const string_view> &list)
	:selection{0}
	{
		assert(this->none());
		for(const auto &key : list)
			this->set(indexof<tuple>(key), true);
	}

	include(const std::initializer_list<const string_view> &list)
	:include(vector_view<const string_view>(list))
	{}
};

/// Construct this class with a list of keys you want to deselect for a given
/// tuple. This constructs a bitset representing the keys of the tuple and
/// lights the bits which are not in the list.
template<class tuple>
struct ircd::json::keys<tuple>::exclude
:selection
{
	exclude(const vector_view<const string_view> &list)
	:selection{}
	{
		assert(this->all());
		for(const auto &key : list)
			this->set(indexof<tuple>(key), false);
	}

	exclude(const std::initializer_list<const string_view> &list)
	:exclude(vector_view<const string_view>(list))
	{}
};

//
// selection
//

template<class tuple>
template<class it_a,
	     class it_b>
constexpr it_a
ircd::json::keys<tuple>::selection::transform(it_a it,
                                              const it_b end)
const
{
	this->until([&it, &end](auto&& key)
	{
		if(it == end)
			return false;

		*it = key;
		++it;
		return true;
	});

	return it;
}

template<class tuple>
template<class closure>
constexpr void
ircd::json::keys<tuple>::selection::for_each(closure &&function)
const
{
	this->until([&function](auto&& key)
	{
		function(key);
		return true;
	});
}

template<class tuple>
template<class closure>
constexpr bool
ircd::json::keys<tuple>::selection::until(closure &&function)
const
{
	for(size_t i(0); i < tuple::size(); ++i)
		if(this->test(i))
			if(!function(key<tuple>(i)))
				return false;

	return true;
}

//
// keys
//

template<class tuple>
constexpr
ircd::json::keys<tuple>::keys(const selection &selection)
{
	selection.transform(this->begin(), this->end());
}

template<class tuple>
ircd::json::keys<tuple>::operator
vector_view<const string_view>()
const
{
	return { this->data(), this->count() };
}

template<class tuple>
size_t
ircd::json::keys<tuple>::count()
const
{
	size_t i(0);
	for(; i < this->size(); ++i)
		if(!(*this)[i])
			break;

	return i;
}
