// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include "db.h"

//
// Mutex
//

static_assert
(
	sizeof(rocksdb::port::Mutex) <= sizeof(pthread_mutex_t) + 1,
	"link-time punning of our structure won't work if the structure is larger "
	"than the one rocksdb has assumed space for."
);

__attribute__((externally_visible, noinline))
rocksdb::port::Mutex::Mutex()
noexcept
{
	#ifdef RB_DEBUG_DB_PORT_
	if(unlikely(!ctx::current))
		return;

	log::debug
	{
		db::log, "mutex %lu %p CTOR", ctx::id(), this
	};
	#endif
}

__attribute__((externally_visible, noinline))
rocksdb::port::Mutex::Mutex(bool adaptive)
noexcept
:Mutex{}
{
}

__attribute__((externally_visible, noinline))
rocksdb::port::Mutex::~Mutex()
noexcept
{
	#ifdef RB_DEBUG_DB_PORT_
	if(unlikely(!ctx::current))
		return;

	log::debug
	{
		db::log, "mutex %lu %p DTOR", ctx::id(), this
	};
	#endif
}

void
__attribute__((externally_visible, noinline))
rocksdb::port::Mutex::Lock()
noexcept
{
	if(unlikely(!ctx::current))
		return;

	#ifdef RB_DEBUG_DB_PORT
	log::debug
	{
		db::log, "mutex %lu %p LOCK", ctx::id(), this
	};
	#endif

	assert_main_thread();
	const ctx::uninterruptible::nothrow ui;
	mu.lock();
}

void
__attribute__((externally_visible, noinline))
rocksdb::port::Mutex::Unlock()
noexcept
{
	if(unlikely(!ctx::current))
		return;

	#ifdef RB_DEBUG_DB_PORT
	log::debug
	{
		db::log, "mutex %lu %p UNLOCK", ctx::id(), this
	};
	#endif

	assert_main_thread();
	assert(mu.locked());
	const ctx::uninterruptible::nothrow ui;
	mu.unlock();
}

void
__attribute__((externally_visible, noinline))
rocksdb::port::Mutex::AssertHeld()
noexcept
{
	if(unlikely(!ctx::current))
		return;

	assert(mu.locked());
}

//
// RWMutex
//

static_assert
(
	sizeof(rocksdb::port::RWMutex) <= sizeof(pthread_rwlock_t),
	"link-time punning of our structure won't work if the structure is larger "
	"than the one rocksdb has assumed space for."
);

__attribute__((externally_visible, noinline))
rocksdb::port::RWMutex::RWMutex()
noexcept
{
	#ifdef RB_DEBUG_DB_PORT_
	if(unlikely(!ctx::current))
		return;

	log::debug
	{
		db::log, "shared_mutex %lu %p CTOR", ctx::id(), this
	};
	#endif
}

__attribute__((externally_visible, noinline))
rocksdb::port::RWMutex::~RWMutex()
noexcept
{
	#ifdef RB_DEBUG_DB_PORT_
	if(unlikely(!ctx::current))
		return;

	log::debug
	{
		db::log, "shared_mutex %lu %p DTOR", ctx::id(), this
	};
	#endif
}

void
__attribute__((externally_visible, noinline))
rocksdb::port::RWMutex::ReadLock()
noexcept
{
	if(unlikely(!ctx::current))
		return;

	#ifdef RB_DEBUG_DB_PORT
	log::debug
	{
		db::log, "shared_mutex %lu %p LOCK SHARED", ctx::id(), this
	};
	#endif

	assert_main_thread();
	const ctx::uninterruptible::nothrow ui;
	mu.lock_shared();
}

void
__attribute__((externally_visible, noinline))
rocksdb::port::RWMutex::WriteLock()
noexcept
{
	if(unlikely(!ctx::current))
		return;

	#ifdef RB_DEBUG_DB_PORT
	log::debug
	{
		db::log, "shared_mutex %lu %p LOCK", ctx::id(), this
	};
	#endif

	assert_main_thread();
	const ctx::uninterruptible::nothrow ui;
	mu.lock();
}

void
__attribute__((externally_visible, noinline))
rocksdb::port::RWMutex::ReadUnlock()
noexcept
{
	if(unlikely(!ctx::current))
		return;

	#ifdef RB_DEBUG_DB_PORT
	log::debug
	{
		db::log, "shared_mutex %lu %p UNLOCK SHARED", ctx::id(), this
	};
	#endif

	assert_main_thread();
	const ctx::uninterruptible::nothrow ui;
	mu.unlock_shared();
}

void
__attribute__((externally_visible, noinline))
rocksdb::port::RWMutex::WriteUnlock()
noexcept
{
	if(unlikely(!ctx::current))
		return;

	#ifdef RB_DEBUG_DB_PORT
	log::debug
	{
		db::log, "shared_mutex %lu %p UNLOCK", ctx::id(), this
	};
	#endif

	assert_main_thread();
	const ctx::uninterruptible::nothrow ui;
	mu.unlock();
}

//
// CondVar
//

static_assert
(
	sizeof(rocksdb::port::CondVar) <= sizeof(pthread_cond_t) + sizeof(void *),
	"link-time punning of our structure won't work if the structure is larger "
	"than the one rocksdb has assumed space for."
);

__attribute__((externally_visible, noinline))
rocksdb::port::CondVar::CondVar(Mutex *mu)
noexcept
:mu{mu}
{
	#ifdef RB_DEBUG_DB_PORT_
	if(unlikely(!ctx::current))
		return;

	log::debug
	{
		db::log, "cond %lu %p %p CTOR", ctx::id(), this, mu
	};
	#endif
}

__attribute__((externally_visible, noinline))
rocksdb::port::CondVar::~CondVar()
noexcept
{
	#ifdef RB_DEBUG_DB_PORT_
	if(unlikely(!ctx::current))
		return;

	log::debug
	{
		db::log, "cond %lu %p %p DTOR", ctx::id(), this, mu
	};
	#endif
}

void
__attribute__((externally_visible, noinline))
rocksdb::port::CondVar::Wait()
noexcept
{
	if(unlikely(!ctx::current))
		return;

	#ifdef RB_DEBUG_DB_PORT
	log::debug
	{
		db::log, "cond %lu %p %p WAIT", ctx::id(), this, mu
	};
	#endif

	assert(mu);
	assert_main_thread();
	mu->AssertHeld();
	const ctx::uninterruptible::nothrow ui;
	cv.wait(mu->mu);
}

// Returns true if timeout occurred
bool
__attribute__((externally_visible, noinline))
rocksdb::port::CondVar::TimedWait(uint64_t abs_time_us)
noexcept
{
	assert(ctx::current);

	#ifdef RB_DEBUG_DB_PORT
	log::debug
	{
		db::log, "cond %lu %p %p WAIT_UNTIL %lu", ctx::id(), this, mu, abs_time_us
	};
	#endif

	assert(mu);
	assert_main_thread();
	mu->AssertHeld();
	const std::chrono::microseconds us(abs_time_us);
	const std::chrono::steady_clock::time_point tp(us);
	const ctx::uninterruptible::nothrow ui;
	return cv.wait_until(mu->mu, tp) == std::cv_status::timeout;
}

void
__attribute__((externally_visible, noinline))
rocksdb::port::CondVar::Signal()
noexcept
{
	if(unlikely(!ctx::current))
		return;

	#ifdef RB_DEBUG_DB_PORT
	log::debug
	{
		db::log, "cond %lu %p %p NOTIFY", ctx::id(), this, mu
	};
	#endif

	assert_main_thread();
	const ctx::uninterruptible::nothrow ui;
	cv.notify_one();
}

void
__attribute__((externally_visible, noinline))
rocksdb::port::CondVar::SignalAll()
noexcept
{
	if(unlikely(!ctx::current))
		return;

	#ifdef RB_DEBUG_DB_PORT
	log::debug
	{
		db::log, "cond %lu %p %p BROADCAST", ctx::id(), this, mu
	};
	#endif

	assert_main_thread();
	const ctx::uninterruptible::nothrow ui;
	cv.notify_all();
}
