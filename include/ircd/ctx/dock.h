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

	void terminate(dock &) noexcept;
	void interrupt(dock &) noexcept;
	void notify(dock &) noexcept;
}

/// dock is a condition variable which has no requirement for locking because
/// the context system does not require mutual exclusion for coherence.
///
class ircd::ctx::dock
{
	struct continuation;
	using predicate = std::function<bool ()>;

	list q;

  public:
	bool empty() const noexcept;
	size_t size() const noexcept;
	bool waiting(const ctx &) const noexcept;

	bool wait_until(const system_point, const predicate &);
	bool wait_until(const system_point);

	template<class duration> bool wait_for(const duration, const predicate &);
	template<class duration> bool wait_for(const duration);

	void wait(const predicate &);
	void wait();

	void terminate_all() noexcept;
	void interrupt_all() noexcept;
	void notify_all() noexcept;
	void notify_one() noexcept;
	void notify() noexcept;
};

namespace ircd::ctx
{
	template<> bool dock::wait_for(const microseconds, const predicate &);
	template<> bool dock::wait_for(const microseconds);
}

struct [[gnu::visibility("internal")]]
ircd::ctx::dock::continuation
{
	dock *const d;

	continuation(dock *);
	~continuation() noexcept;
};

/// Wake up the next context waiting on the dock
inline void
ircd::ctx::dock::notify_one()
noexcept
{
	if(q.empty())
		return;

	ircd::ctx::notify(*q.front());
}

template<class duration>
inline bool
ircd::ctx::dock::wait_for(const duration dur,
                          const predicate &pred)
{
	static_assert(!std::is_same<duration, microseconds>());
	return wait_for(duration_cast<microseconds>(dur), pred);
}

template<class duration>
inline bool
ircd::ctx::dock::wait_for(const duration dur)
{
	static_assert(!std::is_same<duration, microseconds>());
	return wait_for(duration_cast<microseconds>(dur));
}


/// The number of contexts waiting in the queue.
inline size_t
ircd::ctx::dock::size()
const noexcept
{
    return q.size();
}

/// The number of contexts waiting in the queue.
inline bool
ircd::ctx::dock::empty()
const noexcept
{
    return q.empty();
}

inline void
ircd::ctx::notify(dock &dock)
noexcept
{
	dock.notify();
}

inline void
ircd::ctx::interrupt(dock &dock)
noexcept
{
	dock.interrupt_all();
}

inline void
ircd::ctx::terminate(dock &dock)
noexcept
{
	dock.terminate_all();
}
