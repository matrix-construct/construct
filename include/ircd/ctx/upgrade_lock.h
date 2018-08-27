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
#define HAVE_IRCD_CTX_UPGRADE_LOCK_H

namespace ircd::ctx
{
	template<class mutex> struct upgrade_lock;
}

/// Upgrade a lock from a shared reader to become a unique writer. This is a
/// two phase process. The first phase is the upgrade, which does not immediately
/// interfere with the shared readers but only one upgrade can exist at a time.
///
/// The second phase is further upgrading the upgrade lock to a unique-lock
/// which blocks out the readers from reacquiring their shared-locks. This lock
/// then has exclusive access to the critical section.
///
/// Upon release of the second phase the readers can reacquire their
/// shared-lock again. Upon release of the first phase another writer can
/// acquire an upgrade-lock. Sometimes it is desirable to not release the first
/// phase to ensure nothing is ABA'ed by another writer while allowing the
/// readers to continue consuming.
///
template<class mutex>
struct ircd::ctx::upgrade_lock
{
	mutex *m {nullptr};

	bool owns_lock() const;

	void lock();
	bool try_lock();
	template<class duration> bool try_lock_for(duration&&);
	template<class time_point> bool try_lock_until(time_point&&);
	void unlock();

	mutex *release() noexcept;

	template<class r, class p> upgrade_lock(mutex &, const std::chrono::time_point<r, p> &);
	template<class r, class p> upgrade_lock(mutex &, const std::chrono::duration<r, p> &);
	upgrade_lock(mutex &, std::try_to_lock_t);
	upgrade_lock(mutex &, std::defer_lock_t);
	upgrade_lock(mutex &, std::adopt_lock_t);
	explicit upgrade_lock(mutex &);
	upgrade_lock() = default;
	upgrade_lock(upgrade_lock &&) noexcept;
	upgrade_lock(const upgrade_lock &) = delete;
	~upgrade_lock() noexcept;
};

namespace ircd
{
	using ctx::upgrade_lock;
}

template<class mutex>
ircd::ctx::upgrade_lock<mutex>::upgrade_lock(mutex &m)
:m{&m}
{
	lock();
}

template<class mutex>
ircd::ctx::upgrade_lock<mutex>::upgrade_lock(mutex &m,
                                             std::defer_lock_t)
:m{&m}
{
}

template<class mutex>
ircd::ctx::upgrade_lock<mutex>::upgrade_lock(mutex &m,
                                             std::adopt_lock_t)
:m{&m}
{
	if(!m->upgrade())
		lock();
}

template<class mutex>
template<class rep,
         class period>
ircd::ctx::upgrade_lock<mutex>::upgrade_lock(mutex &m,
                                             const std::chrono::duration<rep, period> &rel)
:m{&m}
{
	try_lock_for(rel);
}

template<class mutex>
template<class rep,
         class period>
ircd::ctx::upgrade_lock<mutex>::upgrade_lock(mutex &m,
                                             const std::chrono::time_point<rep, period> &abs)
:m{&m}
{
	try_lock_until(abs);
}

template<class mutex>
ircd::ctx::upgrade_lock<mutex>::upgrade_lock(upgrade_lock &&other)
noexcept
:m{other.release()}
{
}

template<class mutex>
ircd::ctx::upgrade_lock<mutex>::~upgrade_lock()
noexcept
{
	if(owns_lock())
		unlock();
}

template<class mutex>
mutex *
ircd::ctx::upgrade_lock<mutex>::release()
noexcept
{
	mutex *const m{this->m};
	this->m = nullptr;
	return m;
}

template<class mutex>
template<class time_point>
bool
ircd::ctx::upgrade_lock<mutex>::try_lock_until(time_point&& tp)
{
	assert(m);
	return m->try_lock_upgrade_until(std::forward<time_point>(tp));
}

template<class mutex>
template<class duration>
bool
ircd::ctx::upgrade_lock<mutex>::try_lock_for(duration&& d)
{
	assert(m);
	return m->try_lock_upgrade_for(std::forward<duration>(d));
}

template<class mutex>
bool
ircd::ctx::upgrade_lock<mutex>::try_lock()
{
	assert(m);
	return m->try_lock_upgrade();
}

template<class mutex>
void
ircd::ctx::upgrade_lock<mutex>::lock()
{
	assert(m);
	m->lock_upgrade();
}

template<class mutex>
bool
ircd::ctx::upgrade_lock<mutex>::owns_lock()
const
{
	return m? m->upgrade() : false;
}
