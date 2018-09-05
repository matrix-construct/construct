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
#define HAVE_IRCD_DB_DATABASE_CACHE_H

// This file is not part of the standard include stack because it requires
// RocksDB symbols which we cannot forward declare. It is used internally
// and does not need to be included by general users of IRCd.

/// This is an internal wrapper over RocksDB's cache implementation. This is
/// not the public interface to the caches intended to be used by developers
/// of IRCd; that interface is instead found in db/cache.h.
struct ircd::db::database::cache
final
:std::enable_shared_from_this<ircd::db::database::cache>
,rocksdb::Cache
{
	using Slice = rocksdb::Slice;
	using Status = rocksdb::Status;
	using deleter = void (*)(const Slice &key, void *value);
	using callback = void (*)(void *, size_t);
	using Statistics = rocksdb::Statistics;

	static const ssize_t DEFAULT_SHARD_BITS;
	static const double DEFAULT_HI_PRIO;
	static const bool DEFAULT_STRICT;

	database *d;
	cache_stats stats;
	std::shared_ptr<rocksdb::Cache> c;

	const char *Name() const noexcept override;
	Status Insert(const Slice &key, void *value, size_t charge, deleter, Handle **, Priority) noexcept override;
	Handle *Lookup(const Slice &key, Statistics *) noexcept override;
	bool Ref(Handle *) noexcept override;
	bool Release(Handle *, bool force_erase) noexcept override;
	void *Value(Handle *) noexcept override;
	void Erase(const Slice &key) noexcept override;
	uint64_t NewId() noexcept override;
	void SetCapacity(size_t capacity) noexcept override;
	void SetStrictCapacityLimit(bool strict_capacity_limit) noexcept override;
	bool HasStrictCapacityLimit() const noexcept override;
	size_t GetCapacity() const noexcept override;
	size_t GetUsage() const noexcept override;
	size_t GetUsage(Handle *) const noexcept override;
	size_t GetPinnedUsage() const noexcept override;
	void DisownData() noexcept override;
	void ApplyToAllCacheEntries(callback, bool thread_safe) noexcept override;
	void EraseUnRefEntries() noexcept override;
	std::string GetPrintableOptions() const noexcept override;
	void TEST_mark_as_data_block(const Slice &key, size_t charge) noexcept override;

	cache(database *const &d, const ssize_t &initial_capacity = -1);
	~cache() noexcept override;
};
