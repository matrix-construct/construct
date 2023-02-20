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
struct ircd::ctx::dock
{
	enum opts :uint;
	struct continuation;
	using predicate = std::function<bool ()>;

  private:
	list q;

  public:
	bool empty() const noexcept;
	size_t size() const noexcept;
	bool waiting(const ctx &) const noexcept;

	bool wait_until(const system_point, const predicate &, const opts = (opts)0);
	bool wait_until(const system_point, const opts = (opts)0);

	template<class duration> bool wait_for(const duration, const predicate &, const opts = (opts)0);
	template<class duration> bool wait_for(const duration, const opts = (opts)0);

	void wait(const predicate &, const opts = (opts)0);
	void wait(const opts = (opts)0);

	void terminate_all() noexcept;
	void interrupt_all() noexcept;
	void notify_all() noexcept;
	void notify_one() noexcept;
	void notify() noexcept;
};

namespace ircd::ctx
{
	template<> bool dock::wait_for(const microseconds, const predicate &, const opts);
	template<> bool dock::wait_for(const microseconds, const opts);
}

struct [[gnu::visibility("internal")]]
ircd::ctx::dock::continuation
{
	dock *const d;
	const opts *const o;

	continuation(dock *, const opts &opts);
	~continuation() noexcept;
};

/// Options. These are bitflags for forward compatibility with unrelated opts
/// even though some flags are exclusive to others.
enum ircd::ctx::dock::opts
:uint
{
	/// No options.
	NONE = 0x00,

	/// Waiting context will add itself to back of queue. This is the default.
	FIFO = 0x01,

	/// Waiting context will add itself to front of queue. The default is FIFO
	/// for fair queuing preventing starvation.
	LIFO = 0x02,

	/// Waiting context will add itself to front if its ID is lower than the
	/// front, otherwise back.
	SORT = 0x04,
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
                          const predicate &pred,
                          const opts opts)
{
	static_assert(!std::is_same<duration, microseconds>());
	return wait_for(duration_cast<microseconds>(dur), pred, opts);
}

template<class duration>
inline bool
ircd::ctx::dock::wait_for(const duration dur,
                          const opts opts)
{
	static_assert(!std::is_same<duration, microseconds>());
	return wait_for(duration_cast<microseconds>(dur), opts);
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
