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
	struct cache_stats;

	// Get our stats
	const cache_stats &stats(const rocksdb::Cache &);
	cache_stats stats(const rocksdb::Cache *const &);

	// Get capacity
	size_t capacity(const rocksdb::Cache &);
	size_t capacity(const rocksdb::Cache *const &);

	// Set capacity
	void capacity(rocksdb::Cache &, const size_t &);
	bool capacity(rocksdb::Cache *const &, const size_t &);

	// Get usage
	size_t usage(const rocksdb::Cache &);
	size_t usage(const rocksdb::Cache *const &);

	// Test if key exists
	bool exists(const rocksdb::Cache &, const string_view &key);
	bool exists(const rocksdb::Cache *const &, const string_view &key);

	// Iterate the cache entries.
	using cache_closure = std::function<void (const const_buffer &)>;
	void for_each(const rocksdb::Cache &, const cache_closure &);
	void for_each(const rocksdb::Cache *const &, const cache_closure &);

	// Manually cache a key/value directly
	bool insert(rocksdb::Cache &, const string_view &key, unique_buffer<const_buffer>);
	bool insert(rocksdb::Cache *const &, const string_view &key, unique_buffer<const_buffer>);

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

/// Our custom cache statistics. Though the rocksdb::Statistics ticker could
/// be instantiated for each cache we have our own counters, and we let the
/// rocksdb cache impl update our database ticker instead for aggregate stats.
struct ircd::db::cache_stats
{
	size_t inserts {0};
	size_t misses {0};
	size_t hits {0};
};
