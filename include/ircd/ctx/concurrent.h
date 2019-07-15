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
#define HAVE_IRCD_CTX_CONCURRENT

namespace ircd::ctx
{
	template<class value> struct concurrent;
}

template<class value>
struct ircd::ctx::concurrent
{
	using closure = std::function<void (value)>;

	pool &p;
	closure c;
	dock d;
	size_t snd {0};
	size_t rcv {0};
	size_t fin {0};
	std::exception_ptr eptr;

	template<class V> void operator()(V&&);
	void wait();

	concurrent(pool &, closure);
	~concurrent() noexcept;
};

template<class value>
ircd::ctx::concurrent<value>::concurrent(pool &p,
                                         closure c)

:p{p}
,c{std::move(c)}
{}

template<class value>
ircd::ctx::concurrent<value>::~concurrent()
noexcept
{
	const uninterruptible::nothrow ui;
	this->wait();
}

template<class value>
void
ircd::ctx::concurrent<value>::wait()
{
	d.wait([this]
	{
		return snd == fin;
	});
}

template<class value>
template<class V>
void
ircd::ctx::concurrent<value>::operator()(V&& v)
{
	++snd;
	p([this, v(std::move(v))]
	{
		++rcv; try
		{
			c(std::move(v));
		}
		catch(...)
		{
			eptr = std::current_exception();
		}

		++fin;
		d.notify_all();
	});

	if(eptr)
		std::rethrow_exception(eptr);
}
