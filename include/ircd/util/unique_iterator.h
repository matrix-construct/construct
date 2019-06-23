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
#define HAVE_IRCD_UTIL_UNIQUE_ITERATOR_H

namespace ircd {
inline namespace util
{
	template<class container,
	         class iterator = typename container::iterator>
	struct unique_iterator;

	template<class container>
	struct unique_const_iterator;
}}

//
// For objects using the pattern of adding their own instance to a container
// in their constructor, storing an iterator as a member, and then removing
// themselves using the iterator in their destructor. It is unsafe to do that.
// Use this instead; or better, use ircd::instance_list<>
//
template<class container,
         class iterator>
struct ircd::util::unique_iterator
{
	container *c;
	iterator it;

	unique_iterator(container &c, iterator it)
	:c{&c}
	,it{std::move(it)}
	{}

	unique_iterator()
	:c{nullptr}
	{}

	unique_iterator(const unique_iterator &) = delete;
	unique_iterator(unique_iterator &&o) noexcept
	:c{std::move(o.c)}
	,it{std::move(o.it)}
	{
		o.c = nullptr;
	}

	unique_iterator &operator=(const unique_iterator &) = delete;
	unique_iterator &operator=(unique_iterator &&o) noexcept
	{
		this->~unique_iterator();
		c = std::move(o.c);
		it = std::move(o.it);
		o.c = nullptr;
		return *this;
	}

	~unique_iterator() noexcept
	{
		if(c)
			c->erase(it);
	}
};

template<class container>
struct ircd::util::unique_const_iterator
:unique_iterator<container, typename container::const_iterator>
{
	using iterator_type = typename container::const_iterator;
	using unique_iterator<container, iterator_type>::unique_iterator;
};
