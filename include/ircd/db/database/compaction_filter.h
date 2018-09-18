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
#define HAVE_IRCD_DB_DATABASE_COMPACTION_FILTER_H

// This file is not part of the standard include stack because it requires
// RocksDB symbols which we cannot forward declare. It is used internally
// and does not need to be included by general users of IRCd.

struct ircd::db::database::compaction_filter
final
:rocksdb::CompactionFilter
{
	using Slice = rocksdb::Slice;

	column *c;
	database *d;
	db::compactor user;

	const char *Name() const override;
	bool IgnoreSnapshots() const override;
	Decision FilterV2(const int level, const Slice &key, const ValueType v, const Slice &oldval, std::string *newval, std::string *skipuntil) const override;

	compaction_filter(column *const &c, db::compactor);
	~compaction_filter() noexcept override;
};
