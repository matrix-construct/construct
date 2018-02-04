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
#define HAVE_IRCD_CTX_PEEK_H

namespace ircd::ctx
{
	template<class T> class peek;
}

/// Device for a context to share data on its stack with others while yielding
///
/// The peek yields a context while other contexts examine the object pointed
/// to in the peek. This allows a producing context to construct something
/// on its stack and then wait for the consuming contexts to do something with
/// that data before the producer resumes and potentially destroys the data.
/// This creates a very simple and lightweight single-producer/multi-consumer
/// queue mechanism using only context switching.
///
/// Consumers get one chance to safely peek the data when a call to wait()
/// returns. Once the consumer context yields again for any reason the data is
/// potentially invalid. The data can only be peeked once by the consumer
/// because the second call to wait() will yield until the next data is
/// made available by the producer, not the same data.
///
/// Producers will share an object during the call to notify(). Once the call
/// to notify() returns all consumers have peeked the data and the producer is
/// free to destroy it.
///
template<class T>
class ircd::ctx::peek
{
	T *t {nullptr};
	dock a, b;

	bool ready() const;

  public:
	size_t waiting() const;

	// Consumer interface;
	template<class time_point> T &wait_until(time_point&&);
	template<class duration> T &wait_for(const duration &);
	T &wait();

	// Producer interface;
	void notify(T &);

	peek() = default;
	~peek() noexcept;
};

template<class T>
ircd::ctx::peek<T>::~peek()
noexcept
{
	assert(!waiting());
}

template<class T>
void
ircd::ctx::peek<T>::notify(T &t)
{
	const unwind afterward{[this]
	{
		assert(a.empty());
		this->t = nullptr;
		if(!b.empty())
		{
			b.notify_all();
			yield();
		}
	}};

	assert(b.empty());
	this->t = &t;
	a.notify_all();
	yield();
}

template<class T>
T &
ircd::ctx::peek<T>::wait()
{
	b.wait([this]
	{
		return !ready();
	});

	a.wait([this]
	{
		return ready();
	});

	assert(t != nullptr);
	return *t;
}

template<class T>
template<class duration>
T &
ircd::ctx::peek<T>::wait_for(const duration &dur)
{
	return wait_until(now<steady_point>() + dur);
}

template<class T>
template<class time_point>
T &
ircd::ctx::peek<T>::wait_until(time_point&& tp)
{
	if(!b.wait_until(tp, [this]
	{
		return !ready();
	}))
		throw timeout();

	if(!a.wait_until(tp, [this]
	{
		return ready();
	}))
		throw timeout();

	assert(t != nullptr);
	return *t;
}

template<class T>
size_t
ircd::ctx::peek<T>::waiting()
const
{
	return a.size() + b.size();
}

template<class T>
bool
ircd::ctx::peek<T>::ready()
const
{
	return t != nullptr;
}
