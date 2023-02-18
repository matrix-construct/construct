// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
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
	uint64_t sequence(const database::snapshot &) noexcept;  // Sequence of a snapshot
	uint64_t sequence(const rocksdb::Snapshot *) noexcept;
}

/// Database snapshot object. Maintaining this object will maintain a
/// consistent state of access to the database at the sequence number
/// from when it's acquired.
struct ircd::db::database::snapshot
{
	std::shared_ptr<const rocksdb::Snapshot> s;

  public:
	operator const rocksdb::Snapshot *() const;

	explicit operator bool() const;
	bool operator !() const;

	snapshot() = default;
	explicit snapshot(database &);
	snapshot(const snapshot &) = default;
	snapshot &operator=(const snapshot &) = default;
	~snapshot() noexcept;
};

inline bool
ircd::db::database::snapshot::operator!()
const
{
	return !s;
}

inline ircd::db::database::snapshot::operator
bool()
const
{
	return bool(s);
}

inline ircd::db::database::snapshot::operator
const rocksdb::Snapshot *()
const
{
	return s.get();
}

inline uint64_t
ircd::db::sequence(const database::snapshot &s)
noexcept
{
	const rocksdb::Snapshot *const rs(s);
	return sequence(rs);
}
