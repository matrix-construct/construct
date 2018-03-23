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
#define HAVE_IRCD_DB_DATABASE_TXN_H

// This file is not part of the standard include stack because it requires
// RocksDB symbols which we cannot forward declare. It is used internally
// and does not need to be included by general users of IRCd.

struct ircd::db::txn::handler
:rocksdb::WriteBatch::Handler
{
	using Status = rocksdb::Status;
	using Slice = rocksdb::Slice;

	const database &d;
	const std::function<bool (const delta &)> &cb;
	bool _continue {true};

	Status callback(const delta &) noexcept;
	Status callback(const uint32_t &, const op &, const Slice &a, const Slice &b) noexcept;

	bool Continue() noexcept override;
	Status MarkRollback(const Slice &xid) noexcept override;
	Status MarkCommit(const Slice &xid) noexcept override;
	Status MarkEndPrepare(const Slice &xid) noexcept override;
	Status MarkBeginPrepare() noexcept override;

	Status MergeCF(const uint32_t cfid, const Slice &, const Slice &) noexcept override;
	Status SingleDeleteCF(const uint32_t cfid, const Slice &) noexcept override;
	Status DeleteRangeCF(const uint32_t cfid, const Slice &, const Slice &) noexcept override;
	Status DeleteCF(const uint32_t cfid, const Slice &) noexcept override;
	Status PutCF(const uint32_t cfid, const Slice &, const Slice &) noexcept override;

	handler(const database &d,
	        const std::function<bool (const delta &)> &cb)
	:d{d}
	,cb{cb}
	{}
};
