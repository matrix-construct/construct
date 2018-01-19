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
#define HAVE_IRCD_DB_DATABASE_ENV_WRITABLE_FILE_H

// This file is not part of the standard include stack because it requires
// RocksDB symbols which we cannot forward declare. It is used internally
// and does not need to be included by general users of IRCd.

struct ircd::db::database::env::writable_file final
:rocksdb::WritableFile
{
	using Status = rocksdb::Status;
	using Slice = rocksdb::Slice;
	using IOPriority = rocksdb::Env::IOPriority;

	database &d;
	std::unique_ptr<rocksdb::WritableFile> defaults;

	Status Append(const Slice& data) noexcept override;
	Status PositionedAppend(const Slice& data, uint64_t offset) noexcept override;
	Status Truncate(uint64_t size) noexcept override;
	Status Close() noexcept override;
	Status Flush() noexcept override;
	Status Sync() noexcept override;
	Status Fsync() noexcept override;
	bool IsSyncThreadSafe() const noexcept override;
	void SetIOPriority(IOPriority pri) noexcept override;
	IOPriority GetIOPriority() noexcept override;
	uint64_t GetFileSize() noexcept override;
	void GetPreallocationStatus(size_t* block_size, size_t* last_allocated_block) noexcept override;
	size_t GetUniqueId(char* id, size_t max_size) const noexcept override;
	Status InvalidateCache(size_t offset, size_t length) noexcept override;
	void SetPreallocationBlockSize(size_t size) noexcept override;
	void PrepareWrite(size_t offset, size_t len) noexcept override;
	Status Allocate(uint64_t offset, uint64_t len) noexcept override;
	Status RangeSync(uint64_t offset, uint64_t nbytes) noexcept override;

	writable_file(database *const &d, const std::string &name, const EnvOptions &, std::unique_ptr<WritableFile> defaults);
	~writable_file() noexcept;
};
