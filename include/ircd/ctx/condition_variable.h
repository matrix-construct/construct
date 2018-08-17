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
#define HAVE_IRCD_CTX_CONDITION_VARIABLE_H

namespace ircd::ctx
{
	struct condition_variable;
}

class ircd::ctx::condition_variable
{
	list q;

	void notify(ctx &) noexcept;

  public:
	bool empty() const;
	size_t size() const;

	template<class lock, class time_point, class predicate> bool wait_until(lock &, time_point&& tp, predicate&& pred);
	template<class lock, class time_point> std::cv_status wait_until(lock &, time_point&& tp);

	template<class lock, class duration, class predicate> bool wait_for(lock &, const duration &dur, predicate&& pred);
	template<class lock, class duration> std::cv_status wait_for(lock &, const duration &dur);

	template<class lock, class predicate> void wait(lock &, predicate&& pred);
	template<class lock> void wait(lock &);

	void notify_all() noexcept;
	void notify_one() noexcept;
	void notify() noexcept;
};

/// Wake up the next context waiting on the condition_variable
///
/// Unlike notify_one(), the next context in the queue is repositioned in the
/// back before being woken up for fairness.
inline void
ircd::ctx::condition_variable::notify()
noexcept
{
	ctx *c;
	if(!(c = q.pop_front()))
		return;

	q.push_back(c);
	notify(*c);
}

/// Wake up the next context waiting on the condition_variable
inline void
ircd::ctx::condition_variable::notify_one()
noexcept
{
	if(!q.empty())
		notify(*q.front());
}

/// Wake up all contexts waiting on the condition_variable.
///
/// We post all notifications without requesting direct context
/// switches. This ensures everyone gets notified in a single
/// transaction without any interleaving during this process.
inline void
ircd::ctx::condition_variable::notify_all()
noexcept
{
	q.for_each([](ctx &c)
	{
		ircd::ctx::notify(c);
	});
}

template<class lock>
void
ircd::ctx::condition_variable::wait(lock &l)
{
	assert(current);
	const unwind remove{[this]
	{
		q.remove(current);
	}};

	q.push_back(current);
	const unlock_guard<lock> ul{l};
	ircd::ctx::wait();
}

template<class lock,
         class predicate>
void
ircd::ctx::condition_variable::wait(lock &l,
                                    predicate&& pred)
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
		const unlock_guard<lock> ul{l};
		ircd::ctx::wait();
	}
	while(!pred());
}

/// Returns true if notified; false if timed out
template<class lock,
         class duration>
std::cv_status
ircd::ctx::condition_variable::wait_for(lock &l,
                                        const duration &dur)
{
	static const duration zero(0);

	assert(current);
	const unwind remove{[this]
	{
		q.remove(current);
	}};

	q.push_back(current);
	const unlock_guard<lock> ul{l};
	return ircd::ctx::wait<std::nothrow_t>(dur) > zero?
		std::cv_status::no_timeout:
		std::cv_status::timeout;
}

/// Returns true if predicate passed; false if timed out
template<class lock,
         class duration,
         class predicate>
bool
ircd::ctx::condition_variable::wait_for(lock &l,
                                        const duration &dur,
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
		bool expired;
		{
			const unlock_guard<lock> ul{l};
			expired = ircd::ctx::wait<std::nothrow_t>(dur) <= zero;
		};

		if(pred())
			return true;

		if(expired)
			return false;
	}
	while(1);
}

/// Returns true if notified; false if timed out
template<class lock,
         class time_point>
std::cv_status
ircd::ctx::condition_variable::wait_until(lock &l,
                                          time_point&& tp)
{
	assert(current);
	const unwind remove{[this]
	{
		q.remove(current);
	}};

	q.push_back(current);
	const unlock_guard<lock> ul{l};
	return ircd::ctx::wait_until<std::nothrow_t>(tp)?
		std::cv_status::timeout:
		std::cv_status::no_timeout;
}

/// Returns true if predicate passed; false if timed out
template<class lock,
         class time_point,
         class predicate>
bool
ircd::ctx::condition_variable::wait_until(lock &l,
                                          time_point&& tp,
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
		bool expired;
		{
			const unlock_guard<lock> ul{l};
			expired = ircd::ctx::wait_until<std::nothrow_t>(tp);
		};

		if(pred())
			return true;

		if(expired)
			return false;
	}
	while(1);
}

inline void
ircd::ctx::condition_variable::notify(ctx &ctx)
noexcept
{
	// This branch handles condition_variable.notify() being called from outside
	// the context system. If a context is currently running we can make a direct
	// context-switch with yield(ctx), otherwise notify(ctx) enqueues the context.

	if(current)
		ircd::ctx::yield(ctx);
	else
		ircd::ctx::notify(ctx);
}

/// The number of contexts waiting in the queue.
inline size_t
ircd::ctx::condition_variable::size()
const
{
	return q.size();
}

/// The number of contexts waiting in the queue.
inline bool
ircd::ctx::condition_variable::empty()
const
{
	return q.empty();
}
