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
#define HAVE_IRCD_DB_DATABASE_ROCKSDB_H

/// Forward declarations for rocksdb because we do not include it here.
///
/// These are forward declarations to objects we may carry a pointer to.
/// Users of ircd::db should not have to deal directly with these types.
///
namespace rocksdb
{
	struct DB;
	struct Cache;
	struct Options;
	struct DBOptions;
	struct ColumnFamilyOptions;
	struct PlainTableOptions;
	struct BlockBasedTableOptions;
	struct Iterator;
	struct ColumnFamilyHandle;
	struct WriteBatch;
	struct Slice;
	struct PinnableSlice;
	struct Checkpoint;
	struct SstFileManager;
	struct PerfContext;
	struct IOStatsContext;
	struct LiveFileMetaData;
	struct SstFileMetaData;
	struct SstFileWriter;
	struct TableProperties;
	struct LogFile;
	struct Status;
}

//
// Misc utils
//
namespace ircd::db
{
	// Forward slice suite; consider rocksdb::Slice similar to string_view
	rocksdb::Slice slice(const string_view &);
	string_view slice(const rocksdb::Slice &);
	const char *data(const rocksdb::Slice &);
	size_t size(const rocksdb::Slice &);

	// Forward PinnableSlice suite; this inherits from Slice, but the forward
	// interface doesn't know that here.
	string_view slice(const rocksdb::PinnableSlice &);
	const char *data(const rocksdb::PinnableSlice &);
	size_t size(const rocksdb::PinnableSlice &);

	// Iterator suite.
	bool valid(const rocksdb::Iterator &);
	bool operator!(const rocksdb::Iterator &);
	string_view key(const rocksdb::Iterator &);
	string_view val(const rocksdb::Iterator &);
}

inline bool
ircd::db::operator!(const rocksdb::Iterator &it)
{
	return !valid(it);
}
