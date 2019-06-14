// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#if __has_include("db/write_thread.h")
	#define ROCKSDB_PLATFORM_POSIX
	#include "db/write_thread.h"
#endif

#ifndef STORAGE_ROCKSDB_INCLUDE_DB_H_
	#warning "The RocksDB source code is not available. Cannot interpose bugfixes for db/write_thread.h."
#endif

///////////////////////////////////////////////////////////////////////////////
//
// This unit exists to mitigate an instance of a bug in RocksDB documented by
// https://github.com/facebook/rocksdb/issues/4654. In summary, some RocksDB
// code makes direct use of std::mutex and std::condition_variable unlike the
// rest of RocksDB code which uses the rocksdb::port and rocksdb::Env wrapper
// interfaces. We have adapted the latter to work with ircd::ctx userspace
// threading (see: db_port.cc and db_env.cc), but the former is a direct
// interface to kernel pthreads which are incompatible in this context.
//
// Our mitigation is made possible by dynamic linking. It is a legitimate use
// of runtime interposition as stated in official documentation for this exact
// purpose: overriding buggy functions in library dependencies.
//
// This unit overrides a class member function in rocksdb::WriteThread which
// originally made use of pthread primitives to handle two threads contending
// for write access in RocksDB's single-writer design. This function is entered
// by additional threads after a first thread is an established "write leader."
// These additional threads wait until a state bitmask satisfies them so they
// can continue. This waiting is accomplished with an std::condition_variable
// which tells the kernel to stop the thread until satisfied. Since we are not
// using kernel-driven threads, this is a deadlock.
//
// Our solution is not ideal, but operates effectively and is rarely invoked
// given our prior mitigations to eliminate write contention inside RocksDB.
// This code may be hit during simultaneous flush and compaction jobs, which is
// a rare event with our configuration. When hit, our yield loop tends to have
// one iteration. Our solution using a coarse yield, though, is not ideal: the
// waiting ircd::ctx is not actually being notified of a state mask change
// because the write-leader is notifying an std::condition_variable which leads
// to nowhere. Our coarse yield simply puts the waiting ircd::ctx at the back
// of the ircd::ios queue (similar to a sched_yield(2)) and hopes that the
// write-leader has finished its task and updated the state mask before the
// waiter reaches the front of the queue to check. In the worst-case, the
// waiter will requeue itself to check the state mask too often. We can make
// further improvements here by also overriding the notification mechanism
// as well, but we do not feel it's required at this time, as noted.
//
// Unfortunately, we are overriding a C++ class member function to achieve this.
// Class member functions require the correct class definition. This means we
// require a copy of the RocksDB source code to be accessible at build-time. If
// no copy of the RocksDB source is found when this unit is built, it is not an
// error, but the mitigation will not be made and the result will be vulnerable
// to this deadlock.

#ifdef STORAGE_ROCKSDB_INCLUDE_DB_H_
uint8_t
__attribute__((externally_visible))
rocksdb::WriteThread::BlockingAwaitState(Writer *const w,
                                         uint8_t goal_mask)
{
	// Create the class member mutex and cv where it's expected by
	// rocksdb callers
	w->CreateMutex();

	auto state(w->state.load(std::memory_order_acquire));
	assert(state != STATE_LOCKED_WAITING);
	if((state & goal_mask) == 0 && w->state.compare_exchange_strong(state, STATE_LOCKED_WAITING))
	{
		size_t yields(0);
		while((state = w->state.load(std::memory_order_relaxed)) == STATE_LOCKED_WAITING)
		{
			ircd::ctx::yield();
			++yields;
		}

		// Since we're using a coarse ctx::yield() it's theoretically possible
		// that our loop can spin out of control. That is highly unlikely,
		// and there is usually not even more than one iteration. Nevertheless
		// we assert to be sure this is working within reason.
		assert(yields < 32);
	}

	assert((state & goal_mask) != 0);
	return state;
}
#endif STORAGE_ROCKSDB_INCLUDE_DB_H_
