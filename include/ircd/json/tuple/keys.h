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
:std::bitset<T::size()>
{
	template<class closure> constexpr bool until(closure&&) const;
	template<class closure> constexpr void for_each(closure&&) const;
	template<class it> constexpr it transform(it, const it end) const;
	bool has(const string_view &) const;
	void set(const string_view &, const bool & = true);
	void set(const size_t &, const bool & = true);

	// Note the default all-bits set.
	constexpr selection(const uint64_t &val = -1)
	:std::bitset<T::size()>{val}
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
	include(const vector_view<const string_view> &list)
	:selection{0}
	{
		assert(this->none());
		for(const auto &key : list)
			this->set(key, true);
	}

	include(const std::initializer_list<const string_view> &list)
	:include(vector_view<const string_view>(list))
	{}
};

/// Construct this class with a list of keys you want to deselect for a given
/// tuple. This constructs a bitset representing the keys of the tuple and
/// lights the bits which are not in the list.
template<class T>
struct ircd::json::keys<T>::exclude
:selection
{
	exclude(const vector_view<const string_view> &list)
	:selection{}
	{
		assert(this->all());
		for(const auto &key : list)
			this->set(key, false);
	}

	exclude(const std::initializer_list<const string_view> &list)
	:exclude(vector_view<const string_view>(list))
	{}
};

//
// selection
//

template<class T>
void
ircd::json::keys<T>::selection::set(const string_view &key,
                                    const bool &val)
{
	this->set(json::indexof<T>(key), val);
}

template<class T>
void
ircd::json::keys<T>::selection::set(const size_t &pos,
                                    const bool &val)
{
	this->std::bitset<T::size()>::set(pos, val);
}

template<class T>
bool
ircd::json::keys<T>::selection::has(const string_view &key)
const
{
	return this->test(json::indexof<T>(key));
}

template<class T>
template<class it>
constexpr it
ircd::json::keys<T>::selection::transform(it i,
                                          const it end)
const
{
	this->until([&i, &end](auto&& key)
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
constexpr void
ircd::json::keys<T>::selection::for_each(closure&& function)
const
{
	this->until([&function](auto&& key)
	{
		function(key);
		return true;
	});
}

template<class T>
template<class closure>
constexpr bool
ircd::json::keys<T>::selection::until(closure&& function)
const
{
	for(size_t i(0); i < T::size(); ++i)
		if(this->test(i))
			if(!function(key<T>(i)))
				return false;

	return true;
}

//
// keys
//

template<class T>
ircd::json::keys<T>::keys(const selection &selection)
{
	selection.transform(this->begin(), this->end());
}

template<class T>
ircd::json::keys<T>::operator
vector_view<const string_view>()
const
{
	return { this->data(), this->count() };
}

template<class T>
bool
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
size_t
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
