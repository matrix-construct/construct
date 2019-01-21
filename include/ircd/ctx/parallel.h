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
	using closure = std::function<void (arg)>;

	pool *p {nullptr};
	vector_view<arg> a;
	closure c;
	dock d;
	std::exception_ptr eptr;
	ushort snd {0};
	ushort rcv {0};

	bool done() const;
	bool avail() const;
	void wait_done();
	void wait_avail();
	void rethrow_any_exception();
	void receiver() noexcept;
	void sender() noexcept;

  public:
	size_t nextpos() const;

	void operator()();
	void operator()(const arg &a);

	parallel(pool &, const vector_view<arg> &, closure);
	parallel(parallel &&) = delete;
	parallel(const parallel &) = delete;
	~parallel() noexcept;
};

template<class arg>
ircd::ctx::parallel<arg>::parallel(pool &p,
                                   const vector_view<arg> &a,
                                   closure c)
:p{&p}
,a{a}
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
	rethrow_any_exception();
	assert(avail());
	this->a.at(nextpos()) = a;
	sender();
	wait_avail();
}

template<class arg>
void
ircd::ctx::parallel<arg>::operator()()
{
	rethrow_any_exception();
	assert(avail());
	sender();
	wait_avail();
}

template<class arg>
size_t
ircd::ctx::parallel<arg>::nextpos()
const
{
	return snd % a.size();
}

template<class arg>
void
ircd::ctx::parallel<arg>::sender()
noexcept
{
	auto &p(*this->p);
	auto func
	{
		std::bind(&parallel::receiver, this)
	};

	++snd;
	assert(snd > rcv);
	if(likely(p.size()))
		p(std::move(func));
	else
		func();
}

template<class arg>
void
ircd::ctx::parallel<arg>::receiver()
noexcept
{
	assert(snd > rcv);
	const auto pos
	{
		rcv % this->a.size()
	};

	if(!this->eptr) try
	{
		c(this->a.at(pos));
	}
	catch(...)
	{
		this->eptr = std::current_exception();
	}

	++rcv;
	d.notify_one();
	assert(snd >= rcv);
}

template<class arg>
void
ircd::ctx::parallel<arg>::rethrow_any_exception()
{
	if(likely(!this->eptr))
		return;

	wait_done();
	const auto eptr(this->eptr);
	this->eptr = {};
	std::rethrow_exception(eptr);
}

template<class arg>
void
ircd::ctx::parallel<arg>::wait_avail()
{
	d.wait([this]
	{
		return avail();
	});
}

template<class arg>
void
ircd::ctx::parallel<arg>::wait_done()
{
	d.wait([this]
	{
		return done();
	});
}

template<class arg>
bool
ircd::ctx::parallel<arg>::avail()
const
{
	assert(snd >= rcv);
	assert(snd - rcv <= ssize_t(a.size()));
	return snd - rcv < ssize_t(a.size());
}

template<class arg>
bool
ircd::ctx::parallel<arg>::done()
const
{
	assert(snd >= rcv);
	assert(snd - rcv <= ssize_t(a.size()));
	return snd - rcv == 0;
}
