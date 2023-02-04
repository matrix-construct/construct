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

namespace ircd {
inline namespace util
{
	template<class K,
	         class T,
	         class C = std::less<K>>
	struct instance_multimap;
}}

/// See instance_list for purpose and overview.
template<class K,
         class T,
         class C>
struct ircd::util::instance_multimap
{
	using value_type = std::pair<const K, T *>;
	using allocator_state_type = ircd::allocator::node<value_type>;
	using allocator_scope = typename allocator_state_type::scope;
	using allocator_type = typename allocator_state_type::allocator;
	using map_type = std::multimap<K, T *, C, allocator_type>;
	using node_type = std::pair<typename map_type::node_type, value_type>;
	using iterator_type = typename map_type::iterator;
	using const_iterator_type = typename map_type::const_iterator;

	static allocator_state_type allocator;
	static map_type map;

  protected:
	node_type node;
	iterator_type it;

	template<class Key>
	instance_multimap(const const_iterator_type &hint, Key&& key);

	template<class Key>
	instance_multimap(Key&& key);

	instance_multimap() = delete;
	instance_multimap(instance_multimap &&) noexcept;
	instance_multimap(const instance_multimap &);
	instance_multimap &operator=(instance_multimap &&) noexcept;
	instance_multimap &operator=(const instance_multimap &);
	~instance_multimap() noexcept;
};

template<class K,
         class T,
         class C>
template<class Key>
inline
ircd::util::instance_multimap<K, T, C>::instance_multimap(Key&& key)
{
	const allocator_scope alloca
	{
		map, this->node
	};

	it = map.emplace(std::forward<Key>(key), static_cast<T *>(this));
}

template<class K,
         class T,
         class C>
template<class Key>
inline
ircd::util::instance_multimap<K, T, C>::instance_multimap(const const_iterator_type &hint,
                                                          Key&& key)
{
	const allocator_scope alloca
	{
		map, this->node
	};

	it = map.emplace_hint(hint, std::forward<Key>(key), static_cast<T *>(this));
}

template<class K,
         class T,
         class C>
inline
ircd::util::instance_multimap<K, T, C>::instance_multimap(instance_multimap &&other)
noexcept
{
	const allocator_scope alloca
	{
		map, this->node
	};

	it = likely(other.it != end(map))?
		map.emplace_hint(other.it, other.it->first, static_cast<T *>(this)):
		end(map);
}

template<class K,
         class T,
         class C>
inline
ircd::util::instance_multimap<K, T, C>::instance_multimap(const instance_multimap &other)
{
	const allocator_scope alloca
	{
		map, this->node
	};

	it = likely(other.it != end(map))?
		map.emplace_hint(other.it, other.it->first, static_cast<T *>(this)):
		end(map);
}

template<class K,
         class T,
         class C>
inline ircd::util::instance_multimap<K, T, C> &
ircd::util::instance_multimap<K, T, C>::operator=(instance_multimap &&other)
noexcept
{
	this->~instance_multimap();

	const allocator_scope alloca
	{
		map, this->node
	};

	it = likely(other.it != end(map))?
		map.emplace_hint(other.it, other.it->first, static_cast<T *>(this)):
		end(map);

	return *this;
}

template<class K,
         class T,
         class C>
inline ircd::util::instance_multimap<K, T, C> &
ircd::util::instance_multimap<K, T, C>::operator=(const instance_multimap &other)
{
	this->~instance_multimap();

	const allocator_scope alloca
	{
		map, this->node
	};

	it = likely(other.it != end(map))?
		map.emplace_hint(other.it, other.it->first, static_cast<T *>(this)):
		end(map);

	return *this;
}

template<class K,
         class T,
         class C>
inline
ircd::util::instance_multimap<K, T, C>::~instance_multimap()
noexcept
{
	if(it != end(map))
		map.erase(it);
}
