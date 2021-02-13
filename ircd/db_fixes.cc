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

#if __has_include("util/file_util.h")
	#include "util/file_util.h"
#elif __has_include("file/file_util.h")
	#include "file/file_util.h"
#endif

#if __has_include("table/block_fetcher.h")
	#include "table/block_fetcher.h"
#endif

#if __has_include("file/sst_file_manager_impl.h")
	#include "file/sst_file_manager_impl.h"
#elif __has_include("util/sst_file_manager_impl.h")
	#include "util/sst_file_manager_impl.h"
#else
	#error "RocksDB file/sst_file_manager_imp.h is not available. Cannot interpose bugfixes."
#endif

#if __has_include("util/thread_local.h")
	#include "util/thread_local.h"
#else
	#error "RocksDB util/thread_local.h is not available. Cannot interpose bugfixes."
#endif

#if __has_include("table/block_based/reader_common.h")
	#include "table/block_based/reader_common.h"
#endif

#include "db_has.h"
#include "db_env.h"

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
// ThreadLocalPtr
//

#if __has_include("util/thread_local.h")

namespace ircd::db::fixes::tls
{
	using key_pair = pair<uint32_t>;

	static uint32_t id_ctr;
	static std::map<uint64_t, void *> map;
	static std::map<uint32_t, rocksdb::UnrefHandler> dtors;
	static ctx::mutex dtor_mutex;

	static key_pair make_key(const uint64_t &);
	static uint64_t make_key(const key_pair &);
}

ircd::db::fixes::tls::key_pair
ircd::db::fixes::tls::make_key(const uint64_t &k)
{
	return
	{
		uint32_t(k >> 32), uint32_t(k)
	};
}

uint64_t
ircd::db::fixes::tls::make_key(const key_pair &k)
{
	uint64_t ret(0UL);
	ret |= k.first;
	ret <<= 32;
	ret |= k.second;
	return ret;
}

void
rocksdb::ThreadLocalPtr::InitSingletons()
{
}

rocksdb::ThreadLocalPtr::ThreadLocalPtr(UnrefHandler handler)
:id_
{
	uint32_t(ircd::db::fixes::tls::id_ctr++)
}
{
	assert(ircd::db::fixes::tls::id_ctr < uint32_t(-1U));

	if(handler)
	{
		using namespace ircd::db::fixes::tls;

		const auto iit
		{
			dtors.emplace(id_, handler)
		};

		assert(iit.second);
	}
}

rocksdb::ThreadLocalPtr::~ThreadLocalPtr()
{
	using namespace ircd::db::fixes::tls;

	const std::lock_guard lock
	{
		dtor_mutex
	};

	UnrefHandler dtor{nullptr};
	{
		const auto it
		{
			dtors.find(id_)
		};

		if(it != end(dtors))
		{
			dtor = it->second;
			dtors.erase(it);
		}
	}

	const uint64_t key[2]
	{
		make_key({id_, 0}),        // lower bound
		make_key({id_ + 1, 0})     // upper bound
	};

	auto it
	{
		map.lower_bound(key[0])
	};

	for(; it != end(map) && it->first < key[1]; it = map.erase(it))
		if(dtor)
			dtor(it->second);
}

void *
rocksdb::ThreadLocalPtr::Get()
const
{
	using namespace ircd::db::fixes::tls;

	const auto ctxid
	{
		ircd::ctx::current?
			ircd::ctx::id():
			0UL
	};

	const auto it
	{
		map.find(make_key({id_, ctxid}))
	};

	return it != end(map)?
		it->second:
		nullptr;
}

void
rocksdb::ThreadLocalPtr::Reset(void *const ptr)
{
	this->Swap(ptr);
}

void *
rocksdb::ThreadLocalPtr::Swap(void *ptr)
{
	using namespace ircd::db::fixes::tls;

	const auto ctxid
	{
		ircd::ctx::current?
			ircd::ctx::id():
			0UL
	};

	const auto iit
	{
		map.emplace(make_key({id_, ctxid}), ptr)
	};

	if(iit.second)
		return nullptr;

	std::swap(iit.first->second, ptr);
	return ptr;
}

bool
rocksdb::ThreadLocalPtr::CompareAndSwap(void *const ptr,
                                        void *&expected)
{
	using namespace ircd::db::fixes::tls;

	const auto ctxid
	{
		ircd::ctx::current?
			ircd::ctx::id():
			0UL
	};

	const auto key
	{
		make_key({id_, ctxid})
	};

	const auto it
	{
		map.lower_bound(key)
	};

	if(it == end(map) || it->first != key)
	{
		if(expected != nullptr)
		{
			expected = nullptr;
			return false;
		}

		map.emplace_hint(it, key, ptr);
		return true;
	}

	if(it->second != expected)
	{
		expected = it->second;
		return false;
	}

	it->second = ptr;
	return true;
}

void
rocksdb::ThreadLocalPtr::Fold(FoldFunc func,
                              void *const res)
{
	ircd::always_assert(false);
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

#if defined(IRCD_DB_BYPASS_CHECKSUM) \
&& (__has_include("util/file_util.h") || __has_include("file/file_util.h") || __has_include("table/block_fetcher.h"))
void
rocksdb::BlockFetcher::CheckBlockChecksum()
{
	//assert(false);
}
#endif

#if defined(IRCD_DB_BYPASS_CHECKSUM) \
&& __has_include("table/block_based/reader_common.h")
rocksdb::Status
rocksdb::VerifyBlockChecksum(ChecksumType type,
                             const char *const data,
                             size_t block_size,
                             const std::string& file_name,
                             uint64_t offset)
{
	return Status::OK();
}
#endif
