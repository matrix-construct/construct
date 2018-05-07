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

namespace ircd::util
{
	template<class T> struct instance_list;
}

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
	static std::list<T *> list;

  protected:
	typename decltype(list)::iterator it;

	instance_list();
	instance_list(instance_list &&);
	instance_list(const instance_list &);
	instance_list &operator=(instance_list &&);
	instance_list &operator=(const instance_list &);
	~instance_list() noexcept;
};

template<class T>
ircd::util::instance_list<T>::instance_list()
:it{list.emplace(end(list), static_cast<T *>(this))}
{}

template<class T>
ircd::util::instance_list<T>::instance_list(instance_list &&other)
:it{list.emplace(end(list), static_cast<T *>(this))}
{}

template<class T>
ircd::util::instance_list<T>::instance_list(const instance_list &other)
:it{list.emplace(end(list), static_cast<T *>(this))}
{}

template<class T>
ircd::util::instance_list<T> &
ircd::util::instance_list<T>::operator=(instance_list &&other)
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
