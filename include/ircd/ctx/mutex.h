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
class ircd::ctx::mutex
{
	bool m;
	dock q;

  public:
	bool locked() const;
	size_t waiting() const;

	bool try_lock();
	template<class time_point> bool try_lock_until(const time_point &);
	template<class duration> bool try_lock_for(const duration &);

	void lock();
	void unlock();

	mutex();
	mutex(mutex &&) noexcept;
	mutex(const mutex &) = delete;
	mutex &operator=(mutex &&) noexcept;
	mutex &operator=(const mutex &) = delete;
	~mutex() noexcept;
};

inline
ircd::ctx::mutex::mutex()
:m{false}
{
}

inline
ircd::ctx::mutex::mutex(mutex &&o)
noexcept
:m{std::move(o.m)}
,q{std::move(o.q)}
{
	o.m = false;
}

inline
ircd::ctx::mutex &
ircd::ctx::mutex::operator=(mutex &&o)
noexcept
{
	this->~mutex();
	m = std::move(o.m);
	q = std::move(o.q);
	o.m = false;
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
	assert(m);
	m = false;
	q.notify_one();
}

inline void
ircd::ctx::mutex::lock()
{
	q.wait([this]
	{
		return !locked();
	});

	m = true;
}

template<class duration>
bool
ircd::ctx::mutex::try_lock_for(const duration &d)
{
	return try_lock_until(steady_clock::now() + d);
}

template<class time_point>
bool
ircd::ctx::mutex::try_lock_until(const time_point &tp)
{
	const bool success
	{
		q.wait_until(tp, [this]
		{
			return !locked();
		})
	};

	if(likely(success))
		m = true;

	return success;
}

inline bool
ircd::ctx::mutex::try_lock()
{
	if(locked())
		return false;

	m = true;
	return true;
}

inline size_t
ircd::ctx::mutex::waiting()
const
{
	return q.size();
}

inline bool
ircd::ctx::mutex::locked()
const
{
	return m;
}
