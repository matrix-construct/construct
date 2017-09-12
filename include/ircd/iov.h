/*
 * charybdis5
 * Copyright (C) 2016 Jason Volk <jason@zemos.net>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice is present in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

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
:i{&iov}
{
	auto &list
	{
		static_cast<iov::list &>(iov)
	};

	auto &list_node
	{
		*static_cast<iov::list_node *>(this)
	};

	const auto &address
	{
		reinterpret_cast<T *>(&list_node)
	};

	list.get_allocator().s->next = address;
	list.emplace_front(std::forward<args>(a)...);
}

template<class T>
ircd::iov<T>::node::~node()
noexcept
{
	i->remove_if([this](const T &x)
	{
		return &x == &static_cast<const T &>(*this);
	});
}

template<class T>
ircd::iov<T>::node::operator
T &()
{
	auto &list_node(static_cast<iov::list_node &>(*this));
	return *list_node._M_valptr(); //TODO: XXX
}

template<class T>
ircd::iov<T>::node::operator
const T &()
const
{
	const auto &list_node(static_cast<const iov::list_node &>(*this));
	return *list_node._M_valptr(); //TODO: XXX
}
