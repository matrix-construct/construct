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
#define HAVE_IRCD_CTX_QUEUE_H

namespace ircd::ctx
{
	template<class T,
	         class A = std::allocator<T>>
	class queue;
}

template<class T,
         class A>
class ircd::ctx::queue
{
	dock d;
	std::deque<T, A> q;
	size_t w {0};

  public:
	size_t empty() const;
	size_t size() const;
	size_t waiting() const;

	// Consumer interface; waits for item and std::move() it off the queue
	template<class time_point> T pop_until(time_point&&);
	template<class duration> T pop_for(const duration &);
	T pop();

	// Producer interface; emplace item on the queue and notify consumer
	template<class... args> void emplace(args&&...);
	void push(const T &);
	void push(T &&) noexcept;

	queue();
	queue(A&& alloc);
	~queue() noexcept;
};

template<class T,
         class A>
ircd::ctx::queue<T, A>::queue()
:q(std::allocator<T>())
{
}

template<class T,
         class A>
ircd::ctx::queue<T, A>::queue(A&& alloc)
:q(std::forward<A>(alloc))
{
}

template<class T,
         class A>
ircd::ctx::queue<T, A>::~queue()
noexcept
{
	assert(q.empty());
}

template<class T,
         class A>
void
ircd::ctx::queue<T, A>::push(T&& t)
noexcept
{
	q.push_back(std::forward<T>(t));
	d.notify();
}

template<class T,
         class A>
void
ircd::ctx::queue<T, A>::push(const T &t)
{
	q.push_back(t);
	d.notify();
}

template<class T,
         class A>
template<class... args>
void
ircd::ctx::queue<T, A>::emplace(args&&... a)
{
	q.emplace(std::forward<args>(a)...);
	d.notify();
}

template<class T,
         class A>
T
ircd::ctx::queue<T, A>::pop()
{
	const scope_count w
	{
		this->w
	};

	d.wait([this]
	{
		return !q.empty();
	});

	assert(!q.empty());
	auto ret(std::move(q.front()));
	q.pop_front();
	return ret;
}

template<class T,
         class A>
template<class duration>
T
ircd::ctx::queue<T, A>::pop_for(const duration &dur)
{
	const scope_count w
	{
		this->w
	};

	const bool ready
	{
		d.wait_for(dur, [this]
		{
			return !q.empty();
		})
	};

	if(!ready)
		throw timeout{};

	assert(!q.empty());
	auto ret(std::move(q.front()));
	q.pop();
	return ret;
}

template<class T,
         class A>
template<class time_point>
T
ircd::ctx::queue<T, A>::pop_until(time_point&& tp)
{
	const scope_count w
	{
		this->w
	};

	const bool ready
	{
		d.wait_until(tp, [this]
		{
			return !q.empty();
		})
	};

	if(!ready)
		throw timeout{};

	assert(!q.empty());
	auto ret(std::move(q.front()));
	q.pop();
	return ret;
}

template<class T,
         class A>
size_t
ircd::ctx::queue<T, A>::waiting()
const
{
	return w;
}

template<class T,
         class A>
size_t
ircd::ctx::queue<T, A>::size()
const
{
	return q.size();
}

template<class T,
         class A>
size_t
ircd::ctx::queue<T, A>::empty()
const
{
	return q.empty();
}
