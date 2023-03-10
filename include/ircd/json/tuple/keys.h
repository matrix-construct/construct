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

/// Array of string literals (in string_views) representing just the keys of a
/// tuple. By default construction all keys are included in the array. A
/// selection construction will only include keys that are selected. Note that
/// the selection construction packs all the chosen keys at the front of the
/// array so you cannot rely on this for a key's index into the tuple.
template<class T>
struct ircd::json::keys
:std::array<string_view, T::size()>
{
	struct selection;
	struct include;
	struct exclude;

	size_t count() const;
	bool has(const string_view &) const;

	operator vector_view<const string_view>() const;

	keys(const selection & = {});
};

/// Selection of keys in a tuple represented by a bitset. Users generally
/// do not construct this class directly. Instead, construct one of the
/// `include` or `exclude` classes which will set these bits appropriately.
template<class T>
struct ircd::json::keys<T>::selection
:util::bitset<T::size()>
{
	template<class closure> constexpr bool for_each(closure&&) const;
	template<class it> constexpr it transform(it, const it end) const;
	constexpr bool has(const string_view &) const;
	constexpr void set(const string_view &, const bool = true);
	constexpr void set(const size_t &, const bool = true);

	// Note the default all-bits set.
	constexpr selection(const uint128_t val = -1)
	:util::bitset<T::size()>{val}
	{}

	static_assert(T::size() <= sizeof(uint64_t) * 8);
};

/// Construct this class with a list of keys you want to select for a given
/// tuple. This constructs a bitset representing the keys of the tuple and
/// lights the bits for your selections.
template<class T>
struct ircd::json::keys<T>::include
:selection
{
	include(const vector_view<const string_view> &keys)
	:selection{0}
	{
		for(const auto &key : keys)
			selection::set(key, true);
	}

	template<class... list>
	consteval include(list&&... keys)
	:selection{0}
	{
		for(auto&& key : {keys...})
			selection::set(key, true);
	}

	constexpr include()
	:selection{0}
	{}
};

/// Construct this class with a list of keys you want to deselect for a given
/// tuple. This constructs a bitset representing the keys of the tuple and
/// lights the bits which are not in the list.
template<class T>
struct ircd::json::keys<T>::exclude
:selection
{
	exclude(const vector_view<const string_view> &keys)
	:selection{}
	{
		for(const auto &key : keys)
			selection::set(key, false);
	}


	template<class... list>
	consteval exclude(list&&... keys)
	:selection{}
	{
		for(auto&& key : {keys...})
			selection::set(key, false);
	}

	constexpr exclude()
	:selection{}
	{}
};

//
// selection
//

template<class T>
inline constexpr void
ircd::json::keys<T>::selection::set(const string_view &key,
                                    const bool val)
{
	selection::set(json::indexof<T>(key), val);
}

template<class T>
inline constexpr void
ircd::json::keys<T>::selection::set(const size_t &pos,
                                    const bool val)
{
	util::bitset<T::size()>::set(pos, val);
}

template<class T>
inline constexpr bool
ircd::json::keys<T>::selection::has(const string_view &key)
const
{
	return util::bitset<T::size()>::test(json::indexof<T>(key));
}

template<class T>
template<class it>
inline constexpr it
ircd::json::keys<T>::selection::transform(it i,
                                          const it end)
const
{
	for_each([&i, &end](auto&& key)
	{
		if(i == end)
			return false;

		*i = key;
		++i;
		return true;
	});

	return i;
}

template<class T>
template<class closure>
inline constexpr bool
ircd::json::keys<T>::selection::for_each(closure&& function)
const
{
	for(size_t i(0); i < T::size(); ++i)
		if(util::bitset<T::size()>::test(i))
			if(!function(key<T>(i)))
				return false;

	return true;
}

//
// keys
//

template<class T>
inline
ircd::json::keys<T>::keys(const selection &selection)
{
	selection.transform(this->begin(), this->end());
}

template<class T>
inline ircd::json::keys<T>::operator
vector_view<const string_view>()
const
{
	return { this->data(), this->count() };
}

template<class T>
inline bool
ircd::json::keys<T>::has(const string_view &key)
const
{
	const auto &start
	{
		this->begin()
	};

	const auto &stop
	{
		start + this->count()
	};

	assert(!empty(key));
	return stop != std::find(start, stop, key);
}

template<class T>
inline size_t
ircd::json::keys<T>::count()
const
{
	size_t i(0);
	#pragma clang loop unroll (disable)
	for(; i < this->size(); ++i)
		if(!(*this)[i])
			break;

	return i;
}
