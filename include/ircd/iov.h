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
#define HAVE_IRCD_IOV_H

namespace ircd
{
	template<class T> struct iov;
}

/// iov (iovector) is a forward list composed on a trip up the stack
/// presenting an iteration of items to a scatter/gather operation like a
/// socket or JSON generator etc. Add items to the iov by constructing nodes
/// on your stack.
///
template<class T>
struct ircd::iov
:std::forward_list<T, typename allocator::node<T>::allocator>
{
	struct node;

	using allocator_state = allocator::node<T>;
	using allocator_type = typename allocator_state::allocator;
	using list = std::forward_list<T, allocator_type>;
	using list_node = typename list::iterator::_Node; //TODO: YYY

	auto size() const
	{
		return std::distance(std::begin(*this), std::end(*this));
	}

	allocator_state a;

	iov(): list{a} {}
};

template<class T>
struct ircd::iov<T>::node
:iov::list_node
{
	iov *const i {nullptr};

	operator const T &() const;
	operator T &();

	node() = default;
	template<class... args> node(iov *const &, args&&...);
	template<class... args> node(iov &, args&&...);
	node(node &&) = delete;
	node(const node &) = delete;
	node &operator=(node &&) = delete;
	node &operator=(const node &) = delete;
	~node() noexcept;
};

template<class T>
template<class... args>
ircd::iov<T>::node::node(iov &iov,
                         args&&... a)
:node
{
	&iov, std::forward<args>(a)...
}
{}

template<class T>
template<class... args>
ircd::iov<T>::node::node(iov *const &iov,
                         args&&... a)
:i{iov}
{
	if(!iov)
		return;

	auto &list
	{
		*static_cast<iov::list *>(iov)
	};

	auto &list_node
	{
		*static_cast<iov::list_node *>(this)
	};

	const auto &address
	{
		reinterpret_cast<uint8_t *>(&list_node)
	};

	list.get_allocator().s->next = reinterpret_cast<T *>(address);
	list.emplace_front(std::forward<args>(a)...);
}

template<class T>
ircd::iov<T>::node::~node()
noexcept
{
	if(i) i->remove_if([this](const T &x)
	{
		return &x == &static_cast<const T &>(*this);
	});
}

template<class T>
ircd::iov<T>::node::operator
T &()
{
	assert(i);
	auto &list_node(static_cast<iov::list_node &>(*this));
	return *list_node._M_valptr(); //TODO: XXX
}

template<class T>
ircd::iov<T>::node::operator
const T &()
const
{
	assert(i);
	const auto &list_node(static_cast<const iov::list_node &>(*this));
	return *list_node._M_valptr(); //TODO: XXX
}
