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
#define HAVE_IRCD_CTX_MUTEX_H

namespace ircd::ctx
{
	class mutex;
}

//
// The mutex only allows one context to lock it and continue,
// additional contexts are queued. This can be used with std::
// locking concepts.
//
struct ircd::ctx::mutex
{
	dock q;
	ctx *m;

	void deadlock_assertion() const noexcept;

  public:
	bool locked() const noexcept;
	size_t waiting() const noexcept;
	bool waiting(const ctx &) const noexcept;

	bool try_lock() noexcept;
	template<class time_point> bool try_lock_until(const time_point &);
	template<class duration> bool try_lock_for(const duration &);

	void lock();
	void unlock();

	mutex() noexcept;
	mutex(mutex &&) noexcept;
	mutex(const mutex &) = delete;
	mutex &operator=(mutex &&) noexcept;
	mutex &operator=(const mutex &) = delete;
	~mutex() noexcept;
};

inline
ircd::ctx::mutex::mutex()
noexcept
:m{nullptr}
{
}

inline
ircd::ctx::mutex::mutex(mutex &&o)
noexcept
:q{std::move(o.q)}
,m{std::move(o.m)}
{
	o.m = nullptr;
}

inline
ircd::ctx::mutex &
ircd::ctx::mutex::operator=(mutex &&o)
noexcept
{
	this->~mutex();
	q = std::move(o.q);
	m = std::move(o.m);
	o.m = nullptr;
	return *this;
}

inline
ircd::ctx::mutex::~mutex()
noexcept
{
	assert(!m);
}

inline void
ircd::ctx::mutex::unlock()
{
	assert(m == current);
	m = nullptr;
	q.notify_one();
}

inline void
ircd::ctx::mutex::lock()
{
	assert(current);
	deadlock_assertion();

	q.wait([this]() noexcept
	{
		return !locked();
	});

	m = current;
}

template<class duration>
inline bool
ircd::ctx::mutex::try_lock_for(const duration &d)
{
	return try_lock_until(system_clock::now() + d);
}

template<class time_point>
inline bool
ircd::ctx::mutex::try_lock_until(const time_point &tp)
{
	assert(current);
	deadlock_assertion();

	const bool success
	{
		q.wait_until(tp, [this]() noexcept
		{
			return !locked();
		})
	};

	if(likely(success))
		m = current;

	return success;
}

inline bool
ircd::ctx::mutex::try_lock()
noexcept
{
	assert(current);
	deadlock_assertion();

	if(locked())
		return false;

	m = current;
	return true;
}

inline bool
ircd::ctx::mutex::waiting(const ctx &c)
const noexcept
{
	return q.waiting(c);
}

inline size_t
ircd::ctx::mutex::waiting()
const noexcept
{
	return q.size();
}

inline bool
ircd::ctx::mutex::locked()
const noexcept
{
	return m;
}

inline void
__attribute__((always_inline, artificial))
ircd::ctx::mutex::deadlock_assertion()
const noexcept
{
	assert(!locked() || m != current);
}
