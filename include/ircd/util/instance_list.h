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
#define HAVE_IRCD_UTIL_INSTANCE_LIST_H

namespace ircd {
inline namespace util
{
	template<class T> struct instance_list;
}}

/// The instance_list pattern is where every instance of a class registers
/// itself in a static list of all instances and removes itself on dtor.
/// IRCd Ex. All clients use instance_list so all clients can be listed for
/// an administrator or be interrupted and disconnected on server shutdown.
///
/// `struct myobj : ircd::instance_list<myobj> {};`
///
/// * The creator of the class no longer has to manually specify what is
/// defined here using unique_iterator; however, one still must provide
/// linkage for the static list.
///
/// * The container pointer used by unique_iterator is eliminated here
/// because of the static list.
///
template<class T>
struct ircd::util::instance_list
{
	using allocator_state = ircd::allocator::node<T *>;
	using allocator_type = typename allocator_state::allocator;
	using list_type = std::list<T *, allocator_type>;
	using list_node = typename list_type::iterator::_Node;

	static allocator_state allocator;
	static list_type list;

  protected:
	list_node node;
	typename list_type::iterator it;

	instance_list();
	instance_list(instance_list &&) noexcept;
	instance_list(const instance_list &);
	instance_list &operator=(instance_list &&) noexcept;
	instance_list &operator=(const instance_list &);
	~instance_list() noexcept;
};

template<class T>
ircd::util::instance_list<T>::instance_list()
{
	const auto &address(reinterpret_cast<uint8_t *>(&node));
	list.get_allocator().s->next = reinterpret_cast<T **>(address);
	it = list.emplace(end(list), static_cast<T *>(this));
	list.get_allocator().s->next = nullptr;
}

template<class T>
ircd::util::instance_list<T>::instance_list(instance_list &&other)
noexcept
{
	const auto &address(reinterpret_cast<uint8_t *>(&node));
	list.get_allocator().s->next = reinterpret_cast<T **>(address);
	it = list.emplace(end(list), static_cast<T *>(this));
	list.get_allocator().s->next = nullptr;
}

template<class T>
ircd::util::instance_list<T>::instance_list(const instance_list &other)
{
	const auto &address(reinterpret_cast<uint8_t *>(&node));
	list.get_allocator().s->next = reinterpret_cast<T **>(address);
	it = list.emplace(end(list), static_cast<T *>(this));
	list.get_allocator().s->next = nullptr;
}

template<class T>
ircd::util::instance_list<T> &
ircd::util::instance_list<T>::operator=(instance_list &&other)
noexcept
{
	assert(it != end(list));
	return *this;
}

template<class T>
ircd::util::instance_list<T> &
ircd::util::instance_list<T>::operator=(const instance_list &other)
{
	assert(it != end(list));
	return *this;
}

template<class T>
ircd::util::instance_list<T>::~instance_list()
noexcept
{
	assert(it != end(list));
	list.erase(it);
}
