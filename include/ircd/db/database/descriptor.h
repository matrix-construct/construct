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
#define HAVE_IRCD_DB_DATABASE_DESCRIPTOR_H

/// Descriptor of a column when opening database. Database must be opened with
/// a consistent set of descriptors describing what will be found upon opening.
struct ircd::db::database::descriptor
{
	using typing = std::pair<std::type_index, std::type_index>;

	/// User given name for this column. Must be consistent.
	std::string name;

	/// User given description of this column; not used by RocksDB
	std::string explain;

	/// Indicate key and value type.
	typing type { typeid(string_view), typeid(string_view) };

	/// RocksDB ColumnFamilyOptions string; can be used for items not
	/// otherwise specified here.
	std::string options {};

	/// User given comparator. We can automatically set this value for
	/// some types given for the type.first typeid; otherwise it must be
	/// set for exotic/unsupported keys.
	db::comparator cmp {};

	/// User given prefix extractor.
	db::prefix_transform prefix {};

	/// Size of the LRU cache for uncompressed blocks
	ssize_t cache_size { -1 };

	/// Size of the LRU cache for compressed blocks
	ssize_t cache_size_comp { -1 };

	/// Bloom filter bits. Filter is still useful even if queries are expected
	/// to always hit on this column; see `expect_queries_hit` option.
	size_t bloom_bits { 10 };

	/// Set this option to true if queries to this column are expected to
	/// find keys that exist. This is useful for columns with keys that
	/// were first found from values in another column, where if the first
	/// column missed there'd be no reason to query this column.
	bool expect_queries_hit { false };

	/// Data block size for uncompressed data. Compression will make the
	/// block smaller when it IO's to and from disk. Smaller blocks may be
	/// more space and query overhead if values exceed this size. Larger
	/// blocks will read and cache unrelated data if values are smaller
	/// than this size.
	size_t block_size { 512 };
};
