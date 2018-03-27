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
	template<class T, class mutex = mutex> class view;
	template<class T> using shared_view = view<T, shared_mutex>;
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
/// The consumers lock the mutex before passing it to the call to wait().
/// wait() returns with a view of the object under lock. Once the consumer
/// releases their lock the viewed object is not safe for them.
///
template<class T,
         class mutex>
class ircd::ctx::view
:public mutex
{
	dock q;
	T *t {nullptr};
	size_t wanting {0};
	size_t waiting {0};

	bool ready() const;

  public:
	// Consumer interface;
	template<class lock, class time_point> T &wait_until(lock &, time_point&&);
	template<class lock, class duration> T &wait_for(lock &, const duration &);
	template<class lock> T &wait(lock &);

	// Producer interface;
	void operator()(T &);

	view() = default;
	~view() noexcept;
};

template<class T,
         class mutex>
ircd::ctx::view<T, mutex>::~view()
noexcept
{
	assert(!waiting);
	assert(!wanting);
}

template<class T,
         class mutex>
void
ircd::ctx::view<T, mutex>::operator()(T &t)
{
	const auto produce{[this]
	(T *const &t, size_t &semaphore)
	{
		q.wait([this, &semaphore]
		{
			return semaphore == 0;
		});

		const std::lock_guard<view> lock{*this};
		this->t = t;
		q.notify_all();
	}};

	produce(&t, wanting);
	produce(nullptr, waiting);
}

template<class T,
         class mutex>
template<class lock>
T &
ircd::ctx::view<T, mutex>::wait(lock &l)
{
	const auto consume{[this, &l]
	(const bool &r, size_t &semaphore)
	{
		const unwind uw{[this, &l, &semaphore]
		{
			--semaphore;
			q.notify_all();
		}};

		++semaphore;
		assert(l.owns_lock());
		const unlock_guard<lock> ul{l};
		q.wait([this, &r]
		{
			return ready() == r;
		});
	}};

	consume(false, wanting);
	consume(true, waiting);
	assert(t != nullptr);
	return *t;
}

template<class T,
         class mutex>
template<class lock,
         class duration>
T &
ircd::ctx::view<T, mutex>::wait_for(lock &l,
                                    const duration &dur)
{
	return wait_until(l, now<steady_point>() + dur);
}

template<class T,
         class mutex>
template<class lock,
         class time_point>
T &
ircd::ctx::view<T, mutex>::wait_until(lock &l,
                                      time_point&& tp)
{
	const auto consume{[this, &l, &tp]
	(const bool &r, size_t &semaphore)
	{
		const unwind uw{[this, &l, &semaphore]
		{
			--semaphore;
			q.notify_all();
		}};

		++semaphore;
		assert(l.owns_lock());
		const unlock_guard<lock> ul{l};
		if(!q.wait_until(tp, [this, &r]
		{
			return ready() == r;
		}))
			throw timeout{};
	}};

	consume(false, wanting);
	consume(true, waiting);
	assert(t != nullptr);
	return *t;
}

template<class T,
         class mutex>
bool
ircd::ctx::view<T, mutex>::ready()
const
{
	return t != nullptr;
}
