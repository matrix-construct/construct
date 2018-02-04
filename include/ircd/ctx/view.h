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
#define HAVE_IRCD_CTX_VIEW_H

namespace ircd::ctx
{
	template<class T> class view;
}

/// Device for a context to share data on its stack with others while yielding
///
/// The view yields a context while other contexts examine the object pointed
/// to in the view. This allows a producing context to construct something
/// on its stack and then wait for the consuming contexts to do something with
/// that data before the producer resumes and potentially destroys the data.
/// This creates a very simple and lightweight single-producer/multi-consumer
/// queue mechanism using only context switching.
///
/// The producer is blocked until all consumers are finished with their view.
/// The consumers acquire the unique_lock before passing it to the call to wait().
/// wait() returns with a view of the object under unique_lock. Once the
/// consumer releases the unique_lock the viewed object is not safe for them.
///
template<class T>
class ircd::ctx::view
:public mutex
{
	T *t {nullptr};
	dock q;
	size_t waiting {0};

	bool ready() const;

  public:
	// Consumer interface;
	template<class time_point> T &wait_until(std::unique_lock<view> &, time_point&&);
	template<class duration> T &wait_for(std::unique_lock<view> &, const duration &);
	T &wait(std::unique_lock<view> &);

	// Producer interface;
	void notify(T &);

	view() = default;
	~view() noexcept;
};

template<class T>
ircd::ctx::view<T>::~view()
noexcept
{
	assert(!waiting);
}

template<class T>
void
ircd::ctx::view<T>::notify(T &t)
{
	if(!waiting)
		return;

	this->t = &t;
	q.notify_all();
	q.wait([this] { return !waiting; });
	const std::lock_guard<view> lock{*this};
	this->t = nullptr;
	assert(!waiting);
	q.notify_all();
}

template<class T>
T &
ircd::ctx::view<T>::wait(std::unique_lock<view> &lock)
{
	for(assert(lock.owns_lock()); ready(); lock.lock())
	{
		lock.unlock();
		q.wait();
	}

	const unwind ul{[this]
	{
		--waiting;
		q.notify_all();
	}};

	for(++waiting; !ready(); lock.lock())
	{
		lock.unlock();
		q.wait();
	}

	assert(t != nullptr);
	return *t;
}

template<class T>
template<class duration>
T &
ircd::ctx::view<T>::wait_for(std::unique_lock<view> &lock,
                             const duration &dur)
{
	return wait_until(now<steady_point>() + dur);
}

template<class T>
template<class time_point>
T &
ircd::ctx::view<T>::wait_until(std::unique_lock<view> &lock,
                               time_point&& tp)
{
	for(assert(lock.owns_lock()); ready(); lock.lock())
	{
		lock.unlock();
		q.wait_until(tp);
	}

	const unwind ul{[this]
	{
		--waiting;
		q.notify_all();
	}};

	for(++waiting; !ready(); lock.lock())
	{
		lock.unlock();
		q.wait_until(tp);
	}

	assert(t != nullptr);
	return *t;
}

template<class T>
bool
ircd::ctx::view<T>::ready()
const
{
	return t != nullptr;
}
