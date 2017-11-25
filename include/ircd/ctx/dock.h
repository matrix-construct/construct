/*
 *  Copyright (C) 2016 Charybdis Development Team
 *  Copyright (C) 2016 Jason Volk <jason@zemos.net>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice is present in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once
#define HAVE_IRCD_CTX_DOCK_H

namespace ircd::ctx
{
	struct dock;
	enum class cv_status;
}

//
// a dock is a condition variable which has no requirement for locking because
// the context system does not require mutual exclusion for coherence, however
// we have to create our own queue here rather than piggyback a mutex's.
//
class ircd::ctx::dock
{
	std::deque<ctx *> q;

	void remove_self();
	void notify(ctx &) noexcept;

  public:
	bool empty() const;
	size_t size() const;

	template<class time_point, class predicate> bool wait_until(time_point&& tp, predicate&& pred);
	template<class time_point> cv_status wait_until(time_point&& tp);

	template<class duration, class predicate> bool wait_for(const duration &dur, predicate&& pred);
	template<class duration> cv_status wait_for(const duration &dur);

	template<class predicate> void wait(predicate&& pred);
	void wait();

	void notify_all() noexcept;
	void notify_one() noexcept;
	void notify() noexcept;

	dock() = default;
	~dock() noexcept;
};

enum class ircd::ctx::cv_status
{
	no_timeout, timeout
};

inline
ircd::ctx::dock::~dock()
noexcept
{
	assert(q.empty());
}

/// Wake up the next context waiting on the dock
///
/// Unlike notify_one(), the next context in the queue is repositioned in the
/// back before being woken up for fairness.
inline void
ircd::ctx::dock::notify()
noexcept
{
	if(q.empty())
		return;

	auto c(q.front());
	q.pop_front();
	q.emplace_back(c);
	notify(*c);
}

/// Wake up the next context waiting on the dock
inline void
ircd::ctx::dock::notify_one()
noexcept
{
	if(q.empty())
		return;

	notify(*q.front());
}

/// Wake up all contexts waiting on the dock.
///
/// We copy the queue and post all notifications without requesting direct context
/// switches. This ensures everyone gets notified in a single transaction without
/// any interleaving during this process.
inline void
ircd::ctx::dock::notify_all()
noexcept
{
	const auto copy(q);
	for(const auto &c : copy)
		ircd::ctx::notify(*c);
}

inline void
ircd::ctx::dock::wait()
{
	const unwind remove
	{
		std::bind(&dock::remove_self, this)
	};

	q.emplace_back(&cur());
	ircd::ctx::wait();
}

template<class predicate>
void
ircd::ctx::dock::wait(predicate&& pred)
{
	if(pred())
		return;

	const unwind remove
	{
		std::bind(&dock::remove_self, this)
	};

	q.emplace_back(&cur()); do
	{
		ircd::ctx::wait();
	}
	while(!pred());
}

template<class duration>
ircd::ctx::cv_status
ircd::ctx::dock::wait_for(const duration &dur)
{
	static const duration zero(0);

	const unwind remove
	{
		std::bind(&dock::remove_self, this)
	};

	q.emplace_back(&cur());
	return ircd::ctx::wait<std::nothrow_t>(dur) > zero? cv_status::no_timeout:
	                                                    cv_status::timeout;
}

template<class duration,
         class predicate>
bool
ircd::ctx::dock::wait_for(const duration &dur,
                          predicate&& pred)
{
	static const duration zero(0);

	if(pred())
		return true;

	const unwind remove
	{
		std::bind(&dock::remove_self, this)
	};

	q.emplace_back(&cur()); do
	{
		const bool expired(ircd::ctx::wait<std::nothrow_t>(dur) <= zero);

		if(pred())
			return true;

		if(expired)
			return false;
	}
	while(1);
}

template<class time_point>
ircd::ctx::cv_status
ircd::ctx::dock::wait_until(time_point&& tp)
{
	const unwind remove
	{
		std::bind(&dock::remove_self, this)
	};

	q.emplace_back(&cur());
	return ircd::ctx::wait_until<std::nothrow_t>(tp)? cv_status::timeout:
	                                                  cv_status::no_timeout;
}

template<class time_point,
         class predicate>
bool
ircd::ctx::dock::wait_until(time_point&& tp,
                            predicate&& pred)
{
	if(pred())
		return true;

	const unwind remove
	{
		std::bind(&dock::remove_self, this)
	};

	q.emplace_back(&cur()); do
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

inline void
ircd::ctx::dock::remove_self()
{
	const auto it(std::find(begin(q), end(q), &cur()));
	assert(it != end(q));
	q.erase(it);
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
