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
	struct queue;
}

template<class T,
         class A>
struct ircd::ctx::queue
{
	using opts = dock::opts;

  private:
	dock d;
	std::deque<T, A> q;
	size_t w {0};

  public:
	size_t empty() const;
	size_t size() const;
	size_t waiting() const;

	// Consumer interface; waits for item and std::move() it off the queue
	template<class time_point> T pop_until(time_point&&, const opts & = (opts)0);
	template<class duration> T pop_for(const duration &, const opts & = (opts)0);
	T pop(const opts & = (opts)0);

	// Producer interface; emplace item on the queue and notify consumer
	template<class... args> void emplace(const opts &, args&&...);
	template<class... args> void emplace(args&&...);

	void push(const opts &, const T &);
	void push(const T &);

	void push(const opts &, T &&) noexcept;
	void push(T &&) noexcept;

	queue();
	queue(A&& alloc);
	~queue() noexcept;
};

template<class T,
         class A>
inline
ircd::ctx::queue<T, A>::queue()
:q(std::allocator<T>())
{
}

template<class T,
         class A>
inline
ircd::ctx::queue<T, A>::queue(A&& alloc)
:q(std::forward<A>(alloc))
{
}

template<class T,
         class A>
inline
ircd::ctx::queue<T, A>::~queue()
noexcept
{
	assert(q.empty());
}

template<class T,
         class A>
inline void
ircd::ctx::queue<T, A>::push(T&& t)
noexcept
{
	static const opts opts {0};
	push(opts, std::forward<T>(t));
}

template<class T,
         class A>
inline void
ircd::ctx::queue<T, A>::push(const opts &opts,
                             T&& t)
noexcept
{
	if(opts & opts::LIFO)
		q.push_front(std::forward<T>(t));
	else
		q.push_back(std::forward<T>(t));

	d.notify();
}

template<class T,
         class A>
inline void
ircd::ctx::queue<T, A>::push(const T &t)
{
	static const opts opts {0};
	push(opts, t);
}

template<class T,
         class A>
inline void
ircd::ctx::queue<T, A>::push(const opts &opts,
                             const T &t)
{
	if(opts & opts::LIFO)
		q.push_front(t);
	else
		q.push_back(t);

	d.notify();
}

template<class T,
         class A>
template<class... args>
inline void
ircd::ctx::queue<T, A>::emplace(args&&... a)
{
	static const opts opts {0};
	emplace(opts, std::forward<args>(a)...);
}

template<class T,
         class A>
template<class... args>
inline void
ircd::ctx::queue<T, A>::emplace(const opts &opts,
                                args&&... a)
{
	if(opts & opts::LIFO)
		q.emplace_front(std::forward<args>(a)...);
	else
		q.emplace_back(std::forward<args>(a)...);

	d.notify();
}

template<class T,
         class A>
inline T
ircd::ctx::queue<T, A>::pop(const opts &opts)
{
	const scope_count w
	{
		this->w
	};

	const auto predicate{[this]() noexcept
	{
		return !q.empty();
	}};

	d.wait(predicate, opts);

	assert(!q.empty());
	auto ret(std::move(q.front()));
	q.pop_front();
	return ret;
}

template<class T,
         class A>
template<class duration>
inline T
ircd::ctx::queue<T, A>::pop_for(const duration &dur,
                                const opts &opts)
{
	const scope_count w
	{
		this->w
	};

	const auto predicate{[this]() noexcept
	{
		return !q.empty();
	}};

	const bool ready
	{
		d.wait_for(dur, predicate, opts)
	};

	if(unlikely(!ready))
		throw timeout{};

	assert(!q.empty());
	auto ret(std::move(q.front()));
	q.pop();
	return ret;
}

template<class T,
         class A>
template<class time_point>
inline T
ircd::ctx::queue<T, A>::pop_until(time_point&& tp,
                                  const opts &opts)
{
	const scope_count w
	{
		this->w
	};

	const auto predicate{[this]() noexcept
	{
		return !q.empty();
	}};

	const bool ready
	{
		d.wait_until(tp, predicate, opts)
	};

	if(unlikely(!ready))
		throw timeout{};

	assert(!q.empty());
	auto ret(std::move(q.front()));
	q.pop();
	return ret;
}

template<class T,
         class A>
inline size_t
ircd::ctx::queue<T, A>::waiting()
const
{
	return w;
}

template<class T,
         class A>
inline size_t
ircd::ctx::queue<T, A>::size()
const
{
	return q.size();
}

template<class T,
         class A>
inline size_t
ircd::ctx::queue<T, A>::empty()
const
{
	return q.empty();
}
