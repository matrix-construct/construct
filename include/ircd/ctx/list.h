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
#define HAVE_IRCD_CTX_LIST_H

namespace ircd::ctx
{
	struct list;
}

/// A special linked-list for contexts. Each ircd::ctx has space for one and
/// only one node on its internal ctx::ctx structure. It can only participate
/// in one ctx::list at a time. This forms the structural basis for mutexes,
/// condition variables and other interleaving primitives which form queues
/// of contexts.
///
/// This device is strictly for context switching purposes. It is minimal,
/// usage is specific to this purpose, and not a general list to be used
/// elsewhere. Furthermore, this is too lightweight for even the
/// ircd::allocator::node strategy. Custom operations are implemented for
/// maximum space efficiency in both the object instance and the ctx::ctx.
///
class ircd::ctx::list
{
	ctx *head {nullptr};
	ctx *tail {nullptr};

	// Get next or prev entry in ctx
	static const ctx *next(const ctx *const &);
	static const ctx *prev(const ctx *const &);
	static ctx *next(ctx *const &);
	static ctx *prev(ctx *const &);

  public:
	const ctx *front() const;
	const ctx *back() const;
	ctx *front();
	ctx *back();

	// iteration
	bool for_each(const std::function<bool (const ctx &)> &) const;
	bool for_each(const std::function<bool (ctx &)> &);
	void for_each(const std::function<void (const ctx &)> &) const;
	void for_each(const std::function<void (ctx &)> &);

	// reverse iteration
	bool rfor_each(const std::function<bool (const ctx &)> &) const;
	bool rfor_each(const std::function<bool (ctx &)> &);
	void rfor_each(const std::function<void (const ctx &)> &) const;
	void rfor_each(const std::function<void (ctx &)> &);

	bool empty() const;
	size_t size() const;

	void push_front(ctx *const & = current);
	void push_back(ctx *const & = current);
	void push(ctx *const & = current);       // push_back

	ctx *pop_front();
	ctx *pop_back();
	ctx *pop();                              // pop_front

	void remove(ctx *const & = current);

	list() = default;
	list(list &&) noexcept;
	list(const list &) = delete;
	list &operator=(list &&) noexcept;
	list &operator=(const list &) = delete;
	~list() noexcept;
};

inline
ircd::ctx::list::list(list &&o)
noexcept
:head{std::move(o.head)}
,tail{std::move(o.tail)}
{
	o.head = nullptr;
	o.tail = nullptr;
}

inline
ircd::ctx::list &
ircd::ctx::list::operator=(list &&o)
noexcept
{
	this->~list();
	std::swap(head, o.head);
	std::swap(tail, o.tail);
	return *this;
}

inline
ircd::ctx::list::~list()
noexcept
{
	assert(empty());
}

inline ircd::ctx::ctx *
ircd::ctx::list::pop()
{
	return pop_front();
}

inline void
ircd::ctx::list::push(ctx *const &c)
{
	push_back(c);
}

inline size_t
ircd::ctx::list::size()
const
{
	size_t i{0};
	for_each([&i](const ctx &)
	{
		++i;
	});

	return i;
}

inline bool
ircd::ctx::list::empty()
const
{
	assert((!head && !tail) || (head && tail));
	return !head;
}

inline ircd::ctx::ctx *
ircd::ctx::list::back()
{
	return tail;
}

inline ircd::ctx::ctx *
ircd::ctx::list::front()
{
	return head;
}

inline const ircd::ctx::ctx *
ircd::ctx::list::back()
const
{
	return tail;
}

inline const ircd::ctx::ctx *
ircd::ctx::list::front()
const
{
	return head;
}
