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
	using predicate = std::function<bool ()>;

	list q;

	void notify(ctx &) noexcept;

  public:
	bool empty() const;
	size_t size() const;

	template<class time_point> bool wait_until(time_point&&, const predicate &);
	template<class time_point> bool wait_until(time_point&&);

	template<class duration> bool wait_for(const duration &, const predicate &);
	template<class duration> bool wait_for(const duration &);

	void wait(const predicate &);
	void wait();

	void notify_all() noexcept;
	void notify_one() noexcept;
	void notify() noexcept;
};

/// Returns true if notified; false if timed out
template<class duration>
bool
ircd::ctx::dock::wait_for(const duration &dur)
{
	static const duration zero(0);

	assert(current);
	const unwind::exceptional renotify{[this]
	{
		notify_one();
	}};

	const unwind remove{[this]
	{
		q.remove(current);
	}};

	q.push_back(current);
	return ircd::ctx::wait<std::nothrow_t>(dur) > zero;
}

/// Returns true if predicate passed; false if timed out
template<class duration>
bool
ircd::ctx::dock::wait_for(const duration &dur,
                          const predicate &pred)
{
	static const duration zero(0);

	if(pred())
		return true;

	assert(current);
	const unwind::exceptional renotify{[this]
	{
		notify_one();
	}};

	const unwind remove{[this]
	{
		q.remove(current);
	}};

	q.push_back(current); do
	{
		const bool expired
		{
			ircd::ctx::wait<std::nothrow_t>(dur) <= zero
		};

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
	const unwind::exceptional renotify{[this]
	{
		notify_one();
	}};

	const unwind remove{[this]
	{
		q.remove(current);
	}};

	q.push_back(current);
	return !ircd::ctx::wait_until<std::nothrow_t>(tp);
}

/// Returns true if predicate passed; false if timed out
template<class time_point>
bool
ircd::ctx::dock::wait_until(time_point&& tp,
                            const predicate &pred)
{
	if(pred())
		return true;

	assert(current);
	const unwind::exceptional renotify{[this]
	{
		notify_one();
	}};

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
