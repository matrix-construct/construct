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
#define HAVE_IRCD_DB_DATABASE_SNAPSHOT_H

// Forward declarations for RocksDB because it is not included here.
namespace rocksdb
{
	struct Snapshot;
}

namespace ircd::db
{
	uint64_t sequence(const database::snapshot &);  // Sequence of a snapshot
	uint64_t sequence(const rocksdb::Snapshot *const &);
}

/// Database snapshot object. Maintaining this object will maintain a
/// consistent state of access to the database at the sequence number
/// from when it's acquired.
struct ircd::db::database::snapshot
{
	std::shared_ptr<const rocksdb::Snapshot> s;

  public:
	operator const rocksdb::Snapshot *() const   { return s.get();                                 }

	explicit operator bool() const               { return bool(s);                                 }
	bool operator !() const                      { return !s;                                      }

	explicit snapshot(database &);
	snapshot() = default;
	~snapshot() noexcept;
};
