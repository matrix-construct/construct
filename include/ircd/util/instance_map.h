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
	using value_type = std::pair<const K, T *>;
	using allocator_state_type = ircd::allocator::node<value_type>;
	using allocator_scope = typename allocator_state_type::scope;
	using allocator_type = typename allocator_state_type::allocator;
	using map_type = std::map<K, T *, C, allocator_type>;
	using node_type = std::pair<typename map_type::node_type, value_type>;
	using iterator_type = typename map_type::iterator;
	using const_iterator_type = typename map_type::const_iterator;

	static allocator_state_type allocator;
	static map_type map;

  protected:
	node_type node;
	iterator_type it;

	template<class Key>
	instance_map(const const_iterator_type &hint, Key&&);

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
	const allocator_scope alloca
	{
		map, this->node
	};

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
ircd::util::instance_map<K, T, C>::instance_map(const const_iterator_type &hint,
                                                Key&& key)
{
	const allocator_scope alloca
	{
		map, this->node
	};

	T *const ptr
	{
		static_cast<T *>(this)
	};

	this->it = map.emplace_hint(hint, std::forward<Key>(key), ptr);

	if(unlikely(this->it->second != ptr))
		throw std::invalid_argument
		{
			"Instance mapping to this key already exists."
		};
}

template<class K,
         class T,
         class C>
inline
ircd::util::instance_map<K, T, C>::instance_map(instance_map &&other)
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
ircd::util::instance_map<K, T, C>::instance_map(const instance_map &other)
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
inline ircd::util::instance_map<K, T, C> &
ircd::util::instance_map<K, T, C>::operator=(instance_map &&other)
noexcept
{
	this->~instance_map();

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
inline ircd::util::instance_map<K, T, C> &
ircd::util::instance_map<K, T, C>::operator=(const instance_map &other)
{
	this->~instance_map();

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
ircd::util::instance_map<K, T, C>::~instance_map()
noexcept
{
	if(it != end(map))
		map.erase(it);
}
