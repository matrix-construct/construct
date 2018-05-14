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
	bool exists(rocksdb::Cache &, const string_view &key);
	bool exists(rocksdb::Cache *const &, const string_view &key);

	// Iterate the cache entries.
	using cache_closure = std::function<void (const string_view &, const size_t &)>;
	void for_each(rocksdb::Cache &, const cache_closure &);
	void for_each(rocksdb::Cache *const &, const cache_closure &);
}
