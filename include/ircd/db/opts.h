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
#define HAVE_IRCD_DB_OPTS_H

namespace ircd::db
{
	struct sopts;
	struct gopts;
	struct options;
}

/// Options for setting (writes)
struct ircd::db::sopts
{
	/// Uses kernel filesystem synchronization after this write (slow).
	bool fsync {false};

	/// Write Ahead Log (WAL) for some crash recovery.
	bool journal {true};

	/// Set to false to fail if write would block.
	bool blocking {true};

	/// Mark for low priority behavior.
	bool prio_low {false};

	/// Mark for high priority behavior.
	bool prio_high {false};
};

/// Options for getting (reads)
struct ircd::db::gopts
{
	/// Keep iter data in memory for iter lifetime (good for lots ++/--).
	bool pin {false};

	/// Fill the cache with results.
	bool cache {true};

	/// Allow query to continue after cache miss.
	bool blocking {true};

	/// Submit requests in parallel (relevant to db::row).
	bool parallel {true};

	/// (prefix_same_as_start); automatic for index columns with pfx.
	bool prefix {false};

	/// (total_order_seek); relevant to index columns.
	bool ordered {false};

	/// Ensures no snapshot is used; this iterator will have the latest data.
	bool tailing {false};

	/// 1 = Throw exceptions more than usual.
	/// 0 = Throw exceptions less than usual.
	int8_t throwing {-1};

	/// 1 = Integrity of data will be checked (overrides conf).
	/// 0 = Checksums will not be checked (overrides conf).
	int8_t checksum {-1};

	/// Readahead bytes.
	size_t readahead {0};

	/// Bounding keys
	const rocksdb::Slice
	*lower_bound {nullptr},
	*upper_bound {nullptr};

	/// Attached snapshot
	database::snapshot snapshot;
};

/// options <-> string
struct ircd::db::options
:std::string
{
	struct map;

	// Output of options structures from this string
	explicit operator rocksdb::Options() const;
	operator rocksdb::DBOptions() const;
	operator rocksdb::ColumnFamilyOptions() const;
	operator rocksdb::PlainTableOptions() const;
	operator rocksdb::BlockBasedTableOptions() const;

	// Input of options structures output to this string
	explicit options(const rocksdb::ColumnFamilyOptions &);
	explicit options(const rocksdb::DBOptions &);
	explicit options(const database::column &);
	explicit options(const database &);

	// Input of options string from user
	options(std::string string)
	:std::string{std::move(string)}
	{}
};

/// options <-> map
struct ircd::db::options::map
:std::unordered_map<std::string, std::string>
{
	// Merge output of options structures from map (map takes precedence).
	rocksdb::DBOptions merge(const rocksdb::DBOptions &) const;
	rocksdb::ColumnFamilyOptions merge(const rocksdb::ColumnFamilyOptions &) const;
	rocksdb::PlainTableOptions merge(const rocksdb::PlainTableOptions &) const;
	rocksdb::BlockBasedTableOptions merge(const rocksdb::BlockBasedTableOptions &) const;

	// Output of options structures from map
	operator rocksdb::DBOptions() const;
	operator rocksdb::ColumnFamilyOptions() const;
	operator rocksdb::PlainTableOptions() const;
	operator rocksdb::BlockBasedTableOptions() const;

	// Convert option string to map
	map(const options &);

	// Input of options map from user
	map(std::unordered_map<std::string, std::string> m = {})
	:std::unordered_map<std::string, std::string>{std::move(m)}
	{}
};
