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
#define HAVE_IRCD_UTIL_INSTANCE_MAP_H

namespace ircd {
inline namespace util
{
	template<class K,
	         class T,
	         class C = std::less<K>>
	struct instance_map;
}}

/// See instance_list for purpose and overview.
template<class K,
         class T,
         class C>
struct ircd::util::instance_map
{
	static std::map<K, T *, C> map;

  protected:
	typename decltype(map)::iterator it;

	template<class Key>
	instance_map(const typename decltype(map)::const_iterator &hint, Key&&);

	template<class Key>
	instance_map(Key&&);

	instance_map(instance_map &&) noexcept;
	instance_map(const instance_map &);
	instance_map &operator=(instance_map &&) noexcept;
	instance_map &operator=(const instance_map &);
	~instance_map() noexcept;
};

template<class K,
         class T,
         class C>
template<class Key>
inline
ircd::util::instance_map<K, T, C>::instance_map(Key&& key)
{
	auto [it, ok]
	{
		map.emplace(std::forward<Key>(key), static_cast<T *>(this))
	};

	if(unlikely(!ok))
		throw std::invalid_argument
		{
			"Instance mapping to this key already exists."
		};

	this->it = std::move(it);
}

template<class K,
         class T,
         class C>
template<class Key>
inline
ircd::util::instance_map<K, T, C>::instance_map(const typename decltype(map)::const_iterator &hint,
                                                Key&& key)
{
	auto [it, ok]
	{
		map.emplace_hint(hint, std::forward<Key>(key), static_cast<T *>(this))
	};

	if(unlikely(!ok))
		throw std::invalid_argument
		{
			"Instance mapping to this key already exists."
		};

	this->it = std::move(it);
}

template<class K,
         class T,
         class C>
inline
ircd::util::instance_map<K, T, C>::instance_map(instance_map &&other)
noexcept
:it
{
	std::move(other.it)
}
{
	if(it != end(map))
	{
		it->second = static_cast<T *>(this);
		other.it = end(map);
	}
}

template<class K,
         class T,
         class C>
inline
ircd::util::instance_map<K, T, C>::instance_map(const instance_map &other)
:it
{
	other.it != end(map)?
		map.emplace_hint(other.it, other.it->first, static_cast<T *>(this)):
		end(map)
}
{}

template<class K,
         class T,
         class C>
inline ircd::util::instance_map<K, T, C> &
ircd::util::instance_map<K, T, C>::operator=(instance_map &&other)
noexcept
{
	this->~instance_map();
	it = std::move(other.it);
	if(it != end(map))
		it->second = static_cast<T *>(this);

	other.it = end(map);
	return *this;
}

template<class K,
         class T,
         class C>
inline ircd::util::instance_map<K, T, C> &
ircd::util::instance_map<K, T, C>::operator=(const instance_map &other)
{
	this->~instance_map();
	it = other.it != end(map)?
		map.emplace_hint(other.it, other.it->first, static_cast<T *>(this)):
		end(map);

	return *this;
}

template<class K,
         class T,
         class C>
inline
ircd::util::instance_map<K, T, C>::~instance_map()
noexcept
{
	if(it != end(map))
		map.erase(it);
}
