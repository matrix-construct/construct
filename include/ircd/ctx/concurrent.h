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
#define HAVE_IRCD_CTX_CONCURRENT_H

namespace ircd::ctx
{
	template<class arg> class concurrent;
}

template<class arg>
struct ircd::ctx::concurrent
{
	using closure = std::function<void (arg &)>;

	pool *p {nullptr};
	vector_view<arg> a;
	std::vector<bool> b;
	closure c;
	dock d;
	std::exception_ptr eptr;
	uint64_t snd {0};           // sends to pool
	uint64_t rcv {0};           // receives by worker
	uint64_t fin {0};           // finished by worker

	bool done() const;
	bool avail() const;
	void wait_done();
	void wait_avail();
	void rethrow_any_exception();
	void receiver(const size_t pos) noexcept;
	void sender(const size_t pos) noexcept;

  public:
	size_t nextpos() const;

	void operator()(const arg &a);

	concurrent(pool &, const vector_view<arg> &, closure);
	concurrent(concurrent &&) = delete;
	concurrent(const concurrent &) = delete;
	~concurrent() noexcept;
};

template<class arg>
ircd::ctx::concurrent<arg>::concurrent(pool &p,
                                       const vector_view<arg> &a,
                                       closure c)
:p{&p}
,a{a}
,b(this->a.size(), false)
,c{std::move(c)}
{
	p.min(this->a.size());
}

template<class arg>
ircd::ctx::concurrent<arg>::~concurrent()
noexcept
{
	const uninterruptible::nothrow ui;
	wait_done();
}

template<class arg>
void
ircd::ctx::concurrent<arg>::operator()(const arg &a)
{
	const uninterruptible ui;
	rethrow_any_exception();
	assert(avail());
	const auto nextpos(this->nextpos());
	assert(nextpos < b.size());
	this->a.at(nextpos) = a;
	assert(this->b.at(nextpos) == false);
	this->b.at(nextpos) = true;
	sender(nextpos);
	wait_avail();
}

template<class arg>
size_t
ircd::ctx::concurrent<arg>::nextpos()
const
{
	const auto it
	{
		std::find(begin(b), end(b), false)
	};

	return std::distance(begin(b), it);
}

template<class arg>
void
ircd::ctx::concurrent<arg>::sender(const size_t pos)
noexcept
{
	assert(pos < b.size());
	auto &p(*this->p);
	auto func
	{
		std::bind(&concurrent::receiver, this, pos) //TODO: alloc
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
ircd::ctx::concurrent<arg>::receiver(const size_t pos)
noexcept
{
	++rcv;
	assert(snd >= rcv);
	if(!this->eptr) try
	{
		c(this->a.at(pos));
	}
	catch(...)
	{
		this->eptr = std::current_exception();
	}

	assert(pos < b.size());
	assert(this->b.at(pos) == true);
	this->b.at(pos) = false;
	assert(rcv > fin);
	++fin;
	d.notify_one();
}

template<class arg>
void
ircd::ctx::concurrent<arg>::rethrow_any_exception()
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
ircd::ctx::concurrent<arg>::wait_avail()
{
	d.wait([this]
	{
		return avail();
	});
}

template<class arg>
void
ircd::ctx::concurrent<arg>::wait_done()
{
	d.wait([this]
	{
		return done();
	});
}

template<class arg>
bool
ircd::ctx::concurrent<arg>::avail()
const
{
	assert(snd >= rcv);
	assert(rcv >= fin);
	assert(snd - rcv <= a.size());
	assert(snd - fin <= a.size());
	return snd - fin < a.size() && nextpos() < a.size();
}

template<class arg>
bool
ircd::ctx::concurrent<arg>::done()
const
{
	assert(snd >= rcv);
	assert(rcv >= fin);
	assert(snd - rcv <= a.size());
	return snd - fin == 0 && nextpos() == 0;
}
