// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

/////////////////////////////////////////////////////////////////////////////////
//
// This unit exists to mitigate instances of bugs in RocksDB and its builds.
// It requires a complete copy of the rocksdb sourcecode to operate; though
// said source does not have to be built.
//

#define ROCKSDB_PLATFORM_POSIX
#define ZSTD_VERSION_NUMBER 0

#if __has_include("db/write_thread.h")
	#include "db/write_thread.h"
#else
	#error "RocksDB db/write_thread.h is not available. Cannot interpose bugfixes."
#endif

#if __has_include("table/block_fetcher.h")
	#include "table/block_fetcher.h"
#else
	#error "RocksDB table/block_fetcher.h is not available. Cannot interpose bugfixes."
#endif

#if __has_include("util/delete_scheduler.h")
	#include "util/delete_scheduler.h"
#elif __has_include("file/delete_scheduler.h")
	#include "file/delete_scheduler.h"
#else
	#error "RocksDB delete_scheduler.h is not available. Cannot interpose bugfixes."
#endif

#if __has_include("util/file_util.h")
	#include "util/file_util.h"
#elif __has_include("file/file_util.h")
	#include "file/file_util.h"
#else
	#error "RocksDB file_util.h is not available. Cannot interpose bugfixes."
#endif

///////////////////////////////////////////////////////////////////////////////
//
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
// This section overrides a class member function in rocksdb::WriteThread which
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
#endif

///////////////////////////////////////////////////////////////////////////////
//
// DeleteScheduler unconditionally starts an std::thread (pthread_create)
// rather than using the rocksdb::Env system. We override this function to
// simply not start that thread.
//

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
#endif

#if __has_include("file/delete_scheduler.h") && defined(IRCD_DB_HAVE_ENV_FILESYSTEM)
rocksdb::DeleteScheduler::DeleteScheduler(Env* env,
                                          FileSystem *fs,
                                          int64_t rate_bytes_per_sec,
                                          Logger* info_log,
                                          SstFileManagerImpl* sst_file_manager,
                                          double max_trash_db_ratio,
                                          uint64_t bytes_max_delete_chunk)
:env_(env),
fs_(fs),
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
#endif

#if __has_include("util/delete_scheduler.h") || __has_include("file/delete_scheduler.h")
rocksdb::DeleteScheduler::~DeleteScheduler()
{
}
#endif

//
// To effectively employ the DeleteScheduler bypass we also interpose the
// function which dispatches deletions to the scheduler to remove the branch
// and directly conduct the deletion.
//

#if __has_include("util/delete_scheduler.h")
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

///////////////////////////////////////////////////////////////////////////////
//
// On platforms where hardware crc32 acceleration is not available and for
// use with valgrind, the crc32 checks over the data can be cumbersome. While
// rocksdb offers options in several places to disable checksum checking, these
// options are not honored in several places internally within rocksdb. Thus
// in case a developer wants to manually bypass the checksumming this stub is
// available.
//

#if (__has_include("util/file_util.h") || __has_include("file/file_util.h")) && defined(IRCD_DB_BYPASS_CHECKSUM)
void
rocksdb::BlockFetcher::CheckBlockChecksum()
{
	//assert(false);
}
#endif
