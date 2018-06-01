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

struct ircd::ctx::condition_variable
{
	dock d;

  public:
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

inline void
ircd::ctx::condition_variable::notify()
noexcept
{
	d.notify();
}

inline void
ircd::ctx::condition_variable::notify_one()
noexcept
{
	d.notify_one();
}

inline void
ircd::ctx::condition_variable::notify_all()
noexcept
{
	d.notify_all();
}

template<class lock>
void
ircd::ctx::condition_variable::wait(lock &l)
{
	const unlock_guard<lock> ul(l);
	d.wait();
}

template<class lock,
         class predicate>
void
ircd::ctx::condition_variable::wait(lock &l,
                                    predicate&& pred)
{
	const unlock_guard<lock> ul(l);
	return d.wait([&l, &pred]
	{
		l.lock();
		const unwind ul{[&l]
		{
			l.unlock();
		}};

		return pred();
	});
}

template<class lock,
         class duration>
std::cv_status
ircd::ctx::condition_variable::wait_for(lock &l,
                                        const duration &dur)
{
	const unlock_guard<lock> ul(l);
	return d.wait_for(dur)?
		std::cv_status::no_timeout:
		std::cv_status::timeout;
}

template<class lock,
         class duration,
         class predicate>
bool
ircd::ctx::condition_variable::wait_for(lock &l,
                                        const duration &dur,
                                        predicate&& pred)
{
	const unlock_guard<lock> ul(l);
	return d.wait_for(dur, [&l, &pred]
	{
		l.lock();
		const unwind ul{[&l]
		{
			l.unlock();
		}};

		return pred();
	});
}

template<class lock,
         class time_point>
std::cv_status
ircd::ctx::condition_variable::wait_until(lock &l,
                                          time_point&& tp)
{
	const unlock_guard<lock> ul(l);
	return d.wait_until(std::forward<time_point>(tp))?
		std::cv_status::no_timeout:
		std::cv_status::timeout;
}

template<class lock,
         class time_point,
         class predicate>
bool
ircd::ctx::condition_variable::wait_until(lock &l,
                                          time_point&& tp,
                                          predicate&& pred)
{
	const unlock_guard<lock> ul(l);
	return d.wait_until(std::forward<time_point>(tp), [&l, &pred]
	{
		l.lock();
		const unwind ul{[&l]
		{
			l.unlock();
		}};

		return pred();
	});
}
