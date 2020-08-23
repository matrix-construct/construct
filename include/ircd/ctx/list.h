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
struct ircd::ctx::list
{
	struct node;
	using closure_bool_const = std::function<bool (const ctx &)>;
	using closure_const = std::function<void (const ctx &)>;
	using closure_bool = std::function<bool (ctx &)>;
	using closure = std::function<void (ctx &)>;

  private:
	ctx *head {nullptr};
	ctx *tail {nullptr};

	static const node &get(const ctx &) noexcept;
	static node &get(ctx &) noexcept;

	// Get next or prev entry in ctx
	static const ctx *next(const ctx *const &) noexcept;
	static const ctx *prev(const ctx *const &) noexcept;
	static ctx *&next(ctx *const &) noexcept;
	static ctx *&prev(ctx *const &) noexcept;

  public:
	const ctx *front() const noexcept;
	const ctx *back() const noexcept;
	ctx *front() noexcept;
	ctx *back() noexcept;

	// iteration
	bool for_each(const closure_bool_const &) const;
	void for_each(const closure_const &) const;
	bool for_each(const closure_bool &);
	void for_each(const closure &);

	// reverse iteration
	bool rfor_each(const closure_bool_const &) const;
	void rfor_each(const closure_const &) const;
	bool rfor_each(const closure_bool &);
	void rfor_each(const closure &);

	bool empty() const noexcept;
	size_t size() const noexcept;

	void push_front(ctx *const & = current) noexcept;
	void push_back(ctx *const & = current) noexcept;
	void push(ctx *const & = current) noexcept;            // push_back

	ctx *pop_front() noexcept;
	ctx *pop_back() noexcept;
	ctx *pop() noexcept;                                   // pop_front

	void remove(ctx *const & = current) noexcept;

	list() = default;
	list(list &&) noexcept;
	list(const list &) = delete;
	list &operator=(list &&) noexcept;
	list &operator=(const list &) = delete;
	~list() noexcept;
};

struct ircd::ctx::list::node
{
	ctx *prev {nullptr};
	ctx *next {nullptr};
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
	head = std::move(o.head);
	tail = std::move(o.tail);
	o.head = nullptr;
	o.tail = nullptr;
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
noexcept
{
	return pop_front();
}

inline void
ircd::ctx::list::push(ctx *const &c)
noexcept
{
	push_back(c);
}

inline bool
ircd::ctx::list::empty()
const noexcept
{
	assert((!head && !tail) || (head && tail));
	return !head;
}

inline ircd::ctx::ctx *
ircd::ctx::list::back()
noexcept
{
	return tail;
}

inline ircd::ctx::ctx *
ircd::ctx::list::front()
noexcept
{
	return head;
}

inline const ircd::ctx::ctx *
ircd::ctx::list::back()
const noexcept
{
	return tail;
}

inline const ircd::ctx::ctx *
ircd::ctx::list::front()
const noexcept
{
	return head;
}

inline ircd::ctx::ctx *&
ircd::ctx::list::prev(ctx *const &c)
noexcept
{
	assert(c);
	return get(*c).prev;
}

inline ircd::ctx::ctx *&
ircd::ctx::list::next(ctx *const &c)
noexcept
{
	assert(c);
	return get(*c).next;
}

inline const ircd::ctx::ctx *
ircd::ctx::list::prev(const ctx *const &c)
noexcept
{
	assert(c);
	return get(*c).prev;
}

inline const ircd::ctx::ctx *
ircd::ctx::list::next(const ctx *const &c)
noexcept
{
	assert(c);
	return get(*c).next;
}
