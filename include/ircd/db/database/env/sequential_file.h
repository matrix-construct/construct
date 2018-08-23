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
#define HAVE_IRCD_DB_DATABASE_ENV_SEQUENTIAL_FILE_H

// This file is not part of the standard include stack because it requires
// RocksDB symbols which we cannot forward declare. It is used internally
// and does not need to be included by general users of IRCd.

struct ircd::db::database::env::sequential_file final
:rocksdb::SequentialFile
{
	using Status = rocksdb::Status;
	using Slice = rocksdb::Slice;

	static const fs::fd::opts default_opts;

	database &d;
	fs::fd::opts opts;
	fs::fd fd;
	off_t offset;

	bool use_direct_io() const noexcept override;
	size_t GetRequiredBufferAlignment() const noexcept override;
	Status InvalidateCache(size_t offset, size_t length) noexcept override;
	Status PositionedRead(uint64_t offset, size_t n, Slice *result, char *scratch) noexcept override;
	Status Read(size_t n, Slice *result, char *scratch) noexcept override;
	Status Skip(uint64_t size) noexcept override;

	sequential_file(database *const &d, const std::string &name, const EnvOptions &);
	~sequential_file() noexcept;
};
