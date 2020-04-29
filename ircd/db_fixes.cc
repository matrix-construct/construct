// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#define ROCKSDB_PLATFORM_POSIX

#if __has_include("table/block_fetcher.h")
	//#define IRCD_DB_BYPASS_CHECKSUM
	#define ZSTD_VERSION_NUMBER 0
	#include "table/block_fetcher.h"
#endif

#if __has_include("util/delete_scheduler.h")
	#include "util/delete_scheduler.h"
#endif

#if __has_include("util/file_util.h")
	#include "util/file_util.h"
#endif

#if __has_include("db/write_thread.h")
	#include "db/write_thread.h"
#endif

///////////////////////////////////////////////////////////////////////////////
//
// This section exists to mitigate an instance of a bug in RocksDB documented
// by https://github.com/facebook/rocksdb/issues/4654. In summary, some RocksDB
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
#if __has_include("db/write_thread.h")
uint8_t
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
#else
#warning "RocksDB source is not available. Cannot interpose bugfixes."
#endif

#if __has_include("util/delete_scheduler.h")
rocksdb::DeleteScheduler::DeleteScheduler(Env* env,
                                          int64_t rate_bytes_per_sec,
                                          Logger* info_log,
                                          SstFileManagerImpl* sst_file_manager,
                                          double max_trash_db_ratio,
                                          uint64_t bytes_max_delete_chunk)
:env_(env),
total_trash_size_(0),
rate_bytes_per_sec_(rate_bytes_per_sec),
pending_files_(0),
bytes_max_delete_chunk_(bytes_max_delete_chunk),
closing_(false),
cv_(&mu_),
info_log_(info_log),
sst_file_manager_(sst_file_manager),
max_trash_db_ratio_(max_trash_db_ratio)
{
	assert(sst_file_manager != nullptr);
	assert(max_trash_db_ratio >= 0);
//	bg_thread_.reset(
//		new port::Thread(&DeleteScheduler::BackgroundEmptyTrash, this));
}

rocksdb::DeleteScheduler::~DeleteScheduler()
{

}
#else
#warning "RocksDB source is not available. Cannot interpose bugfixes."
#endif

#if __has_include("util/file_util.h")
rocksdb::Status
rocksdb::DeleteSSTFile(const ImmutableDBOptions *db_options,
                       const std::string& fname,
                       const std::string& dir_to_sync)
{
	assert(db_options);
	assert(db_options->env);
	return db_options->env->DeleteFile(fname);
}
#endif

#if __has_include("table/block_fetcher.h") && defined(IRCD_DB_BYPASS_CHECKSUM)
void
rocksdb::BlockFetcher::CheckBlockChecksum()
{
	//assert(false);
}
#endif
