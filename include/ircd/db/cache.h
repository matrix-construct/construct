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
#define HAVE_IRCD_DB_CACHE_H

// Interface to a cache. This interface is included in the standard include
// stack by forward declaring the rocksdb::Cache symbol.
//
// The interface overloads on a pointer to a cache for developer convenience.
// This is because caches may not always exist for some entity, and accessors
// will return a null pointer. The pointer overloads here will safely accept
// null pointers and provide the appropriate no-op behavior.

namespace ircd::db
{
	// Get our stats; refer to db/stats.h for ticker ID related.
	const uint64_t &ticker(const rocksdb::Cache &, const uint32_t &ticker_id);
	uint64_t ticker(const rocksdb::Cache *const &, const uint32_t &ticker_id);

	// Get capacity
	size_t capacity(const rocksdb::Cache &);
	size_t capacity(const rocksdb::Cache *const &);

	// Set capacity
	void capacity(rocksdb::Cache &, const size_t &);
	bool capacity(rocksdb::Cache *const &, const size_t &);

	// Get usage
	size_t usage(const rocksdb::Cache &);
	size_t usage(const rocksdb::Cache *const &);

	// Get pinned usage
	size_t pinned(const rocksdb::Cache &);
	size_t pinned(const rocksdb::Cache *const &);

	// Get number of entries
	size_t count(const rocksdb::Cache &);
	size_t count(const rocksdb::Cache *const &);

	// Test if key exists
	bool exists(const rocksdb::Cache &, const string_view &key);
	bool exists(const rocksdb::Cache *const &, const string_view &key);

	// Get charge value for key
	size_t charge(const rocksdb::Cache &, const string_view &key);
	size_t charge(const rocksdb::Cache *const &, const string_view &key);

	// Iterate the cache entries.
	using cache_closure = std::function<void (const const_buffer &)>;
	void for_each(const rocksdb::Cache &, const cache_closure &);
	void for_each(const rocksdb::Cache *const &, const cache_closure &);

	// Manually cache a key/value directly
	bool insert(rocksdb::Cache &, const string_view &key, unique_buffer<const_buffer> &&);
	bool insert(rocksdb::Cache *const &, const string_view &key, unique_buffer<const_buffer> &&);

	// Manually cache a copy of key/value
	bool insert(rocksdb::Cache &, const string_view &key, const string_view &value);
	bool insert(rocksdb::Cache *const &, const string_view &key, const string_view &value);

	// Remove key if it exists
	bool remove(rocksdb::Cache &, const string_view &key);
	bool remove(rocksdb::Cache *const &, const string_view &key);

	// Clear the cache (won't clear entries which are actively referenced)
	void clear(rocksdb::Cache &);
	void clear(rocksdb::Cache *const &);
}

inline void
ircd::db::clear(rocksdb::Cache *const &cache)
{
	if(cache)
		return clear(*cache);
}

inline bool
ircd::db::remove(rocksdb::Cache *const &cache,
                 const string_view &key)
{
	return cache?
		remove(*cache, key):
		false;
}

inline bool
ircd::db::insert(rocksdb::Cache *const &cache,
                 const string_view &key,
                 const string_view &value)
{
	return cache?
		insert(*cache, key, value):
		false;
}

inline bool
ircd::db::insert(rocksdb::Cache *const &cache,
                 const string_view &key,
                 unique_buffer<const_buffer> &&value)
{
	return cache?
		insert(*cache, key, std::move(value)):
		false;
}

inline void
ircd::db::for_each(const rocksdb::Cache *const &cache,
                   const cache_closure &closure)
{
	if(cache)
		for_each(*cache, closure);
}

inline size_t
ircd::db::charge(const rocksdb::Cache *const &cache,
                 const string_view &key)
{
	return cache?
		charge(*cache, key):
		0UL;
}

inline bool
ircd::db::exists(const rocksdb::Cache *const &cache,
                 const string_view &key)
{
	return cache?
		exists(*cache, key):
		false;
}

inline size_t
ircd::db::count(const rocksdb::Cache *const &cache)
{
	return cache?
		count(*cache):
		0UL;
}

inline size_t
ircd::db::pinned(const rocksdb::Cache *const &cache)
{
	return cache?
		pinned(*cache):
		0UL;
}

inline size_t
ircd::db::usage(const rocksdb::Cache *const &cache)
{
	return cache?
		usage(*cache):
		0UL;
}

inline bool
ircd::db::capacity(rocksdb::Cache *const &cache,
                   const size_t &cap)
{
	if(!cache)
		return false;

	capacity(*cache, cap);
	return true;
}

inline size_t
ircd::db::capacity(const rocksdb::Cache *const &cache)
{
	return cache?
		capacity(*cache):
		0UL;
}

inline uint64_t
ircd::db::ticker(const rocksdb::Cache *const &cache,
                 const uint32_t &ticker_id)
{
	return cache?
		ticker(*cache, ticker_id):
		0UL;
}
