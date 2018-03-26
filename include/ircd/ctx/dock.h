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
#define HAVE_IRCD_CTX_DOCK_H

namespace ircd::ctx
{
	struct dock;
}

/// dock is a condition variable which has no requirement for locking because
/// the context system does not require mutual exclusion for coherence.
///
class ircd::ctx::dock
{
	list q;

	void notify(ctx &) noexcept;

  public:
	bool empty() const;
	size_t size() const;

	template<class time_point, class predicate> bool wait_until(time_point&& tp, predicate&& pred);
	template<class time_point> bool wait_until(time_point&& tp);

	template<class duration, class predicate> bool wait_for(const duration &dur, predicate&& pred);
	template<class duration> bool wait_for(const duration &dur);

	template<class predicate> void wait(predicate&& pred);
	void wait();

	void notify_all() noexcept;
	void notify_one() noexcept;
	void notify() noexcept;
};

/// Wake up the next context waiting on the dock
///
/// Unlike notify_one(), the next context in the queue is repositioned in the
/// back before being woken up for fairness.
inline void
ircd::ctx::dock::notify()
noexcept
{
	ctx *c;
	if(!(c = q.pop_front()))
		return;

	q.push_back(c);
	notify(*c);
}

/// Wake up the next context waiting on the dock
inline void
ircd::ctx::dock::notify_one()
noexcept
{
	if(!q.empty())
		notify(*q.front());
}

/// Wake up all contexts waiting on the dock.
///
/// We post all notifications without requesting direct context
/// switches. This ensures everyone gets notified in a single
/// transaction without any interleaving during this process.
inline void
ircd::ctx::dock::notify_all()
noexcept
{
	q.for_each([](ctx &c)
	{
		ircd::ctx::notify(c);
	});
}

inline void
ircd::ctx::dock::wait()
{
	assert(current);
	const unwind remove{[this]
	{
		q.remove(current);
	}};

	q.push_back(current);
	ircd::ctx::wait();
}

template<class predicate>
void
ircd::ctx::dock::wait(predicate&& pred)
{
	if(pred())
		return;

	assert(current);
	const unwind remove{[this]
	{
		q.remove(current);
	}};

	q.push_back(current); do
	{
		ircd::ctx::wait();
	}
	while(!pred());
}

/// Returns true if notified; false if timed out
template<class duration>
bool
ircd::ctx::dock::wait_for(const duration &dur)
{
	static const duration zero(0);

	assert(current);
	const unwind remove{[this]
	{
		q.remove(current);
	}};

	q.push_back(current);
	return ircd::ctx::wait<std::nothrow_t>(dur) > zero;
}

/// Returns true if predicate passed; false if timed out
template<class duration,
         class predicate>
bool
ircd::ctx::dock::wait_for(const duration &dur,
                          predicate&& pred)
{
	static const duration zero(0);

	if(pred())
		return true;

	assert(current);
	const unwind remove{[this]
	{
		q.remove(current);
	}};

	q.push_back(current); do
	{
		const bool expired(ircd::ctx::wait<std::nothrow_t>(dur) <= zero);

		if(pred())
			return true;

		if(expired)
			return false;
	}
	while(1);
}

/// Returns true if notified; false if timed out
template<class time_point>
bool
ircd::ctx::dock::wait_until(time_point&& tp)
{
	assert(current);
	const unwind remove{[this]
	{
		q.remove(current);
	}};

	q.push_back(current);
	return !ircd::ctx::wait_until<std::nothrow_t>(tp);
}

/// Returns true if predicate passed; false if timed out
template<class time_point,
         class predicate>
bool
ircd::ctx::dock::wait_until(time_point&& tp,
                            predicate&& pred)
{
	if(pred())
		return true;

	assert(current);
	const unwind remove{[this]
	{
		q.remove(current);
	}};

	q.push_back(current); do
	{
		const bool expired
		{
			ircd::ctx::wait_until<std::nothrow_t>(tp)
		};

		if(pred())
			return true;

		if(expired)
			return false;
	}
	while(1);
}

inline void
ircd::ctx::dock::notify(ctx &ctx)
noexcept
{
	// This branch handles dock.notify() being called from outside the context system.
	// If a context is currently running we can make a direct context-switch with
	// yield(ctx), otherwise notify(ctx) enqueues the context.

	if(current)
		ircd::ctx::yield(ctx);
	else
		ircd::ctx::notify(ctx);
}

/// The number of contexts waiting in the queue.
inline size_t
ircd::ctx::dock::size()
const
{
	return q.size();
}

/// The number of contexts waiting in the queue.
inline bool
ircd::ctx::dock::empty()
const
{
	return q.empty();
}
