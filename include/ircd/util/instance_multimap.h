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
#define HAVE_IRCD_UTIL_INSTANCE_MULTIMAP_H

namespace ircd::util
{
	template<class K, class T> struct instance_multimap;
}

/// See instance_multimap for purpose and overview.
template<class K,
         class T>
struct ircd::util::instance_multimap
{
	static std::multimap<K, T *> map;

  protected:
	typename decltype(map)::iterator it;

	instance_multimap(const typename decltype(map)::const_iterator &hint, K&& key);
	instance_multimap(K&& key);
	instance_multimap(instance_multimap &&) noexcept;
	instance_multimap(const instance_multimap &);
	instance_multimap &operator=(instance_multimap &&) noexcept;
	instance_multimap &operator=(const instance_multimap &);
	~instance_multimap() noexcept;
};

template<class K,
         class T>
ircd::util::instance_multimap<K, T>::instance_multimap(K&& key)
:it
{
	map.emplace(std::forward<K>(key), static_cast<T *>(this))
}
{}

template<class K,
         class T>
ircd::util::instance_multimap<K, T>::instance_multimap(const typename decltype(map)::const_iterator &hint,
                                                       K&& key)
:it
{
	map.emplace_hint(hint, std::forward<K>(key), static_cast<T *>(this))
}
{}

template<class K,
         class T>
ircd::util::instance_multimap<K, T>::instance_multimap(instance_multimap &&other)
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
         class T>
ircd::util::instance_multimap<K, T>::instance_multimap(const instance_multimap &other)
:it
{
	other.it != end(map)?
		map.emplace_hint(other.it, other.it->first, static_cast<T *>(this)):
		end(map)
}
{}

template<class K,
         class T>
ircd::util::instance_multimap<K, T> &
ircd::util::instance_multimap<K, T>::operator=(instance_multimap &&other)
noexcept
{
	this->~instance_multimap();
	it = std::move(other.it);
	if(it != end(map))
		it->second = static_cast<T *>(this);

	other.it = end(map);
	return *this;
}

template<class K,
         class T>
ircd::util::instance_multimap<K, T> &
ircd::util::instance_multimap<K, T>::operator=(const instance_multimap &other)
{
	this->~instance_multimap();
	it = other.it != end(map)?
		map.emplace_hint(other.it, other.it->first, static_cast<T *>(this)):
		end(map);

	return *this;
}

template<class K,
         class T>
ircd::util::instance_multimap<K, T>::~instance_multimap()
noexcept
{
	if(it != end(map))
		map.erase(it);
}
