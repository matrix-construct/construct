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
#define HAVE_IRCD_CTX_PARALLEL_H

namespace ircd::ctx
{
	template<class arg> class parallel;
}

template<class arg>
struct ircd::ctx::parallel
{
	using closure = std::function<void (arg &)>;

	pool *p {nullptr};
	vector_view<arg> a;
	closure c;
	dock d;
	std::exception_ptr eptr;
	size_t snd {0};
	size_t rcv {0};
	size_t out {0};

  public:
	void wait_avail();
	void wait_done();

	void operator()(const arg &a);

	parallel(pool &, vector_view<arg>, closure);
	~parallel() noexcept;
};

template<class arg>
ircd::ctx::parallel<arg>::parallel(pool &p,
                                   vector_view<arg> a,
                                   closure c)
:p{&p}
,a{std::move(a)}
,c{std::move(c)}
{
	p.min(this->a.size());
}

template<class arg>
ircd::ctx::parallel<arg>::~parallel()
noexcept
{
	const uninterruptible::nothrow ui;
	wait_done();
}

template<class arg>
void
ircd::ctx::parallel<arg>::operator()(const arg &a)
{
	wait_avail();
	if(this->eptr)
		std::rethrow_exception(this->eptr);

	auto &p(*this->p);
	this->a.at(snd++ % this->a.size()) = a;
	out++;
	p([this]()
	mutable
	{
		auto &a
		{
			this->a.at(rcv++ % this->a.size())
		};

		if(!this->eptr) try
		{
			c(a);
		}
		catch(...)
		{
			this->eptr = std::current_exception();
		}

		out--;
		d.notify_one();
	});
}

template<class arg>
void
ircd::ctx::parallel<arg>::wait_avail()
{
	d.wait([this]
	{
		assert(snd >= rcv);
		return out < a.size();
	});
}

template<class arg>
void
ircd::ctx::parallel<arg>::wait_done()
{
	d.wait([this]
	{
		return !out;
	});
}
