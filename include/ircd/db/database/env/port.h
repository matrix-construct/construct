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
#define HAVE_IRCD_DB_DATABASE_ENV_PORT_H

// This file is not part of the standard include stack because it requires
// RocksDB symbols which we cannot forward declare. It is used internally
// and does not need to be included by general users of IRCd.

// !!! EXPERIMENTAL !!!
//
// This file is special; even within the context of embedding RocksDB through
// its env interface. The functionality provided here is NOT done via
// overriding virtual interfaces called by RocksDB like with the rest of env.
// This functionality is deemed too critical for runtime virtual interfaces.
//
// Instead, the definitions we provide override those that RocksDB uses at
// link-time during the compilation of libircd. Interface declarations are not
// provided by RocksDB in its include path either, thus our interface here must
// match the rocksdb::port interface.
//
// Unfortunately if the rocksdb::port interface partially changes and we leave
// unresolved symbols at link time that may be bad, and go silently unnoticed.
//
// !!! EXPERIMENTAL !!!

namespace rocksdb::port
{
	using namespace ircd;

	struct Mutex;
	struct CondVar;
	struct RWMutex;
}

class rocksdb::port::Mutex
{
	friend class CondVar;

	ctx::mutex mu;

  public:
	void Lock();
	void Unlock();
	void AssertHeld();

	Mutex();
	Mutex(bool adaptive);
	Mutex(const Mutex &) = delete;
	Mutex &operator=(const Mutex &) = delete;
	~Mutex();
};

class rocksdb::port::CondVar
{
	Mutex *mu;
	ctx::condition_variable cv;

  public:
	void Wait();
	bool TimedWait(uint64_t abs_time_us); // Returns true if timeout occurred
	void Signal();
	void SignalAll();

	CondVar(Mutex *mu);
	~CondVar();
};

class rocksdb::port::RWMutex
{
	ctx::shared_mutex mu;

  public:
	void ReadLock();
	void WriteLock();
	void ReadUnlock();
	void WriteUnlock();

	RWMutex();
	RWMutex(const RWMutex &) = delete;
	RWMutex &operator=(const RWMutex &) = delete;
	~RWMutex();
};
