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
#define HAVE_IRCD_DB_DATABASE_WAL_FILTER_H

//
// This file is not included in the standard include stack because it contains
// symbols which we cannot declare without RocksDB headers.
//

/// Callback surface for iterating/recovering the write-ahead-log journal.
struct ircd::db::database::wal_filter
:rocksdb::WalFilter
{
	using WriteBatch = rocksdb::WriteBatch;
	using log_number_map = std::map<uint32_t, uint64_t>;
	using name_id_map = std::map<std::string, uint32_t>;

	static conf::item<bool> debug;

	database *d {nullptr};
	log_number_map log_number;
	name_id_map name_id;

	const char *Name() const noexcept override;
	WalProcessingOption LogRecord(const WriteBatch &, WriteBatch *const replace, bool *replaced) const noexcept override;
	WalProcessingOption LogRecordFound(unsigned long long log_nr, const std::string &name, const WriteBatch &, WriteBatch *const replace, bool *replaced) noexcept override;
	void ColumnFamilyLogNumberMap(const log_number_map &, const name_id_map &) noexcept override;

	wal_filter(database *const &);
	~wal_filter() noexcept;
};
