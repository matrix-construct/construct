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
#define HAVE_IRCD_CTX_SHARED_VIEW_H

namespace ircd::ctx
{
	template<class T> class shared_view;
}

/// Device for a context to share data on its stack with others while yielding
///
/// The shared_view yields a context while other contexts examine the object pointed
/// to in the shared_view. This allows a producing context to construct something
/// on its stack and then wait for the consuming contexts to do something with
/// that data before the producer resumes and potentially destroys the data.
/// This creates a very simple and lightweight single-producer/multi-consumer
/// queue mechanism using only context switching.
///
/// The producer is blocked until all consumers are finished with their shared_view.
/// The consumers acquire the shared_lock before passing it to the call to wait().
/// wait() returns with a shared_view of the object under shared_lock. Once the
/// consumer releases the shared_lock the viewed object is not safe for them.
///
template<class T>
class ircd::ctx::shared_view
:public shared_mutex
{
	T *t {nullptr};
	dock q;
	size_t waiting {0};

	bool ready() const;

  public:
	// Consumer interface;
	template<class time_point> T &wait_until(std::shared_lock<shared_view> &, time_point&&);
	template<class duration> T &wait_for(std::shared_lock<shared_view> &, const duration &);
	T &wait(std::shared_lock<shared_view> &);

	// Producer interface;
	void notify(T &);

	shared_view() = default;
	~shared_view() noexcept;
};

template<class T>
ircd::ctx::shared_view<T>::~shared_view()
noexcept
{
	assert(!waiting);
}

template<class T>
void
ircd::ctx::shared_view<T>::notify(T &t)
{
	if(!waiting)
		return;

	this->t = &t;
	q.notify_all();
	q.wait([this] { return !waiting; });
	const std::lock_guard<shared_view> lock{*this};
	this->t = nullptr;
	assert(!waiting);
	q.notify_all();
}

template<class T>
T &
ircd::ctx::shared_view<T>::wait(std::shared_lock<shared_view> &lock)
{
	for(assert(lock.owns_lock_shared()); ready(); lock.lock_shared())
	{
		lock.unlock_shared();
		q.wait();
	}

	const unwind ul{[this]
	{
		--waiting;
		q.notify_all();
	}};

	for(++waiting; !ready(); lock.lock_shared())
	{
		lock.unlock_shared();
		q.wait();
	}

	assert(t != nullptr);
	return *t;
}

template<class T>
template<class duration>
T &
ircd::ctx::shared_view<T>::wait_for(std::shared_lock<shared_view> &lock,
                                    const duration &dur)
{
	return wait_until(now<steady_point>() + dur);
}

template<class T>
template<class time_point>
T &
ircd::ctx::shared_view<T>::wait_until(std::shared_lock<shared_view> &lock,
                                      time_point&& tp)
{
	for(assert(lock.owns_lock_shared()); ready(); lock.lock_shared())
	{
		lock.unlock_shared();
		q.wait_until(tp);
	}

	const unwind ul{[this]
	{
		--waiting;
		q.notify_all();
	}};

	for(++waiting; !ready(); lock.lock_shared())
	{
		lock.unlock_shared();
		q.wait_until(tp);
	}

	assert(t != nullptr);
	return *t;
}

template<class T>
bool
ircd::ctx::shared_view<T>::ready()
const
{
	return t != nullptr;
}
