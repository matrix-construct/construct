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

rocksdb::port::Mutex::Mutex()
noexcept
{
	static_assert
	(
		sizeof(mu_) >= sizeof(mu),
		"Mutex is not fully initialized."
	);

	memset(&mu_, 0x0, sizeof(mu_));

	if constexpr(RB_DEBUG_DB_PORT > 1)
	{
		if(unlikely(!ctx::current))
			return;

		log::debug
		{
			db::log, "mutex %lu %p CTOR",
			ctx::id(),
			this
		};
	}
}

rocksdb::port::Mutex::Mutex(bool adaptive)
noexcept
:Mutex{}
{
}

rocksdb::port::Mutex::~Mutex()
noexcept
{
	if constexpr(RB_DEBUG_DB_PORT > 1)
	{
		if(unlikely(!ctx::current))
			return;

		log::debug
		{
			db::log, "mutex %lu %p DTOR",
			ctx::id(),
			this
		};
	}
}

void
rocksdb::port::Mutex::Lock()
noexcept
{
	if(unlikely(!ctx::current))
		return;

	if constexpr(RB_DEBUG_DB_PORT)
		log::debug
		{
			db::log, "mutex %lu %p LOCK",
			ctx::id(),
			this
		};

	assert_main_thread();
	const ctx::uninterruptible::nothrow ui;
	mu.lock();
}

void
rocksdb::port::Mutex::Unlock()
noexcept
{
	if(unlikely(!ctx::current))
		return;

	if constexpr(RB_DEBUG_DB_PORT)
		log::debug
		{
			db::log, "mutex %lu %p UNLOCK",
			ctx::id(),
			this
		};

	assert_main_thread();
	assert(mu.locked());
	const ctx::uninterruptible::nothrow ui;
	mu.unlock();
}

void
rocksdb::port::Mutex::AssertHeld()
noexcept
{
	assert(!ctx::current || mu.locked());
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

rocksdb::port::RWMutex::RWMutex()
noexcept
{
	static_assert
	(
		sizeof(mu_) >= sizeof(mu),
		"RWMutex is not fully initialized."
	);

	memset(&mu_, 0x0, sizeof(mu_));

	if constexpr(RB_DEBUG_DB_PORT > 1)
	{
		if(unlikely(!ctx::current))
			return;

		log::debug
		{
			db::log, "shared_mutex %lu %p CTOR",
			ctx::id(),
			this
		};
	}
}

rocksdb::port::RWMutex::~RWMutex()
noexcept
{
	if constexpr(RB_DEBUG_DB_PORT > 1)
	{
		if(unlikely(!ctx::current))
			return;

		log::debug
		{
			db::log, "shared_mutex %lu %p DTOR",
			ctx::id(),
			this
		};
	}
}

void
rocksdb::port::RWMutex::ReadLock()
noexcept
{
	if(unlikely(!ctx::current))
		return;

	if constexpr(RB_DEBUG_DB_PORT)
		log::debug
		{
			db::log, "shared_mutex %lu %p LOCK SHARED",
			ctx::id(),
			this
		};

	assert_main_thread();
	const ctx::uninterruptible::nothrow ui;
	mu.lock_shared();
}

void
rocksdb::port::RWMutex::WriteLock()
noexcept
{
	if(unlikely(!ctx::current))
		return;

	if constexpr(RB_DEBUG_DB_PORT)
		log::debug
		{
			db::log, "shared_mutex %lu %p LOCK",
			ctx::id(),
			this
		};

	assert_main_thread();
	const ctx::uninterruptible::nothrow ui;
	mu.lock();
}

void
rocksdb::port::RWMutex::ReadUnlock()
noexcept
{
	if(unlikely(!ctx::current))
		return;

	if constexpr(RB_DEBUG_DB_PORT)
		log::debug
		{
			db::log, "shared_mutex %lu %p UNLOCK SHARED",
			ctx::id(),
			this
		};

	assert_main_thread();
	const ctx::uninterruptible::nothrow ui;
	mu.unlock_shared();
}

void
rocksdb::port::RWMutex::WriteUnlock()
noexcept
{
	if(unlikely(!ctx::current))
		return;

	if constexpr(RB_DEBUG_DB_PORT)
		log::debug
		{
			db::log, "shared_mutex %lu %p UNLOCK",
			ctx::id(),
			this
		};

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

rocksdb::port::CondVar::CondVar(Mutex *mu)
noexcept
{
	static_assert
	(
		sizeof(cv_) >= sizeof(cv),
		"CondVar is not fully initialized."
	);

	memset(&cv_, 0x0, sizeof(cv_));
	this->mu = mu;

	if constexpr(RB_DEBUG_DB_PORT > 1)
	{
		if(unlikely(!ctx::current))
			return;

		log::debug
		{
			db::log, "cond %lu %p %p CTOR",
			ctx::id(),
			this,
			mu
		};
	}
}

rocksdb::port::CondVar::~CondVar()
noexcept
{
	if constexpr(RB_DEBUG_DB_PORT > 1)
	{
		if(unlikely(!ctx::current))
			return;

		log::debug
		{
			db::log, "cond %lu %p %p DTOR",
			ctx::id(),
			this,
			mu
		};
	}
}

void
rocksdb::port::CondVar::Wait()
noexcept
{
	assert(ctx::current);

	if constexpr(RB_DEBUG_DB_PORT)
		log::debug
		{
			db::log, "cond %lu %p %p WAIT",
			ctx::id(),
			this,
			mu
		};

	assert(mu);
	assert_main_thread();
	mu->AssertHeld();
	const ctx::uninterruptible::nothrow ui;
	cv.wait(mu->mu);
}

// Returns true if timeout occurred
bool
rocksdb::port::CondVar::TimedWait(uint64_t abs_time_us)
noexcept
{
	assert(ctx::current);

	if constexpr(RB_DEBUG_DB_PORT)
		log::debug
		{
			db::log, "cond %lu %p %p WAIT_UNTIL %lu",
			ctx::id(),
			this,
			mu,
			abs_time_us
		};

	assert(mu);
	assert_main_thread();
	mu->AssertHeld();
	const std::chrono::microseconds us(abs_time_us);
	const std::chrono::system_clock::time_point tp(us);
	const ctx::uninterruptible::nothrow ui;
	return cv.wait_until(mu->mu, tp) == std::cv_status::timeout;
}

void
rocksdb::port::CondVar::Signal()
noexcept
{
	if constexpr(RB_DEBUG_DB_PORT)
		log::debug
		{
			db::log, "cond %lu %p %p NOTIFY",
			ctx::id(),
			this,
			mu
		};

	assert_main_thread();
	cv.notify_one();
}

void
rocksdb::port::CondVar::SignalAll()
noexcept
{
	if constexpr(RB_DEBUG_DB_PORT)
		log::debug
		{
			db::log, "cond %lu %p %p BROADCAST",
			ctx::id(),
			this,
			mu
		};

	assert_main_thread();
	cv.notify_all();
}
