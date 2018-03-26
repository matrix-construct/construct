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
	template<class T> class queue;
}

template<class T>
class ircd::ctx::queue
{
	struct dock dock;
	std::queue<T> q;

  public:
	auto empty() const                           { return q.empty();                               }
	auto size() const                            { return q.size();                                }

	// Consumer interface; waits for item and std::move() it off the queue
	template<class time_point> T pop_until(time_point&&);
	template<class duration> T pop_for(const duration &);
	T pop();

	// Producer interface; emplace item on the queue and notify consumer
	template<class... args> void emplace(args&&...);
	void push(const T &);
	void push(T &&) noexcept;

	queue() = default;
	~queue() noexcept;
};

template<class T>
ircd::ctx::queue<T>::~queue()
noexcept
{
	assert(q.empty());
}

template<class T>
void
ircd::ctx::queue<T>::push(T &&t)
noexcept
{
	q.push(std::move(t));
	dock.notify();
}

template<class T>
void
ircd::ctx::queue<T>::push(const T &t)
{
	q.push(t);
	dock.notify();
}

template<class T>
template<class... args>
void
ircd::ctx::queue<T>::emplace(args&&... a)
{
	q.emplace(std::forward<args>(a)...);
	dock.notify();
}

template<class T>
T
ircd::ctx::queue<T>::pop()
{
	dock.wait([this]
	{
		return !q.empty();
	});

	const unwind pop([this]
	{
		q.pop();
	});

	auto &ret(q.front());
	return std::move(ret);
}

template<class T>
template<class duration>
T
ircd::ctx::queue<T>::pop_for(const duration &dur)
{
	const bool ready
	{
		dock.wait_for(dur, [this]
		{
			return !q.empty();
		})
	};

	if(!ready)
		throw timeout{};

	const unwind pop([this]
	{
		q.pop();
	});

	auto &ret(q.front());
	return std::move(ret);
}

template<class T>
template<class time_point>
T
ircd::ctx::queue<T>::pop_until(time_point&& tp)
{
	const bool ready
	{
		dock.wait_until(tp, [this]
		{
			return !q.empty();
		})
	};

	if(!ready)
		throw timeout{};

	const unwind pop([this]
	{
		q.pop();
	});

	auto &ret(q.front());
	return std::move(ret);
}
