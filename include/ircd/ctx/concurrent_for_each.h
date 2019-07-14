// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_CTX_CONCURRENT_FOR_EACH

namespace ircd::ctx
{
	template<class value> struct concurrent_for_each;
}

template<class value>
struct ircd::ctx::concurrent_for_each
{
	using function = std::function<void (value &)>;
	using initializer_list = std::initializer_list<value>;

	size_t snd {0};
	size_t rcv {0};
	size_t fin {0};
	std::exception_ptr eptr;

	concurrent_for_each(pool &p, const vector_view<value> &, const function &);
	concurrent_for_each(pool &p, const function &, const initializer_list &);
};

template<class value>
ircd::ctx::concurrent_for_each<value>::concurrent_for_each(pool &p,
                                                           const function &func,
                                                           const initializer_list &list)
:concurrent_for_each
{
	p, list, func
}
{}

template<class value>
ircd::ctx::concurrent_for_each<value>::concurrent_for_each(pool &p,
                                                           const vector_view<value> &list,
                                                           const function &func)
{
	const uninterruptible::nothrow ui;
	latch latch(list.size());
	for(auto it(begin(list)); it != end(list) && !eptr; ++it)
	{
		++snd;
		p([this, &p, &list, &func, &latch]
		{
			++rcv; try
			{
				func(list[rcv - 1]);
			}
			catch(...)
			{
				eptr = std::current_exception();
			}

			++fin;
			latch.count_down();
		});
	}

	latch.wait();

	if(eptr)
		std::rethrow_exception(eptr);
}
