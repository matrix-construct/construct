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
	using WriteLifeTimeHint = rocksdb::Env::WriteLifeTimeHint;

	database &d;
	ctx::mutex mutex;
	rocksdb::EnvOptions env_opts;
	fs::fd::opts opts;
	IOPriority prio {IO_LOW};
	WriteLifeTimeHint hint {WriteLifeTimeHint::WLTH_NOT_SET};
	fs::fd fd;
	size_t _buffer_align;
	size_t logical_offset;
	size_t preallocation_block_size;
	ssize_t preallocation_last_block {-1};

	bool IsSyncThreadSafe() const noexcept override;
	size_t GetUniqueId(char* id, size_t max_size) const noexcept override;
	IOPriority GetIOPriority() noexcept override;
	void SetIOPriority(IOPriority pri) noexcept override;
	WriteLifeTimeHint GetWriteLifeTimeHint() noexcept override;
	void SetWriteLifeTimeHint(WriteLifeTimeHint hint) noexcept override;
	uint64_t GetFileSize() noexcept override;
	void SetPreallocationBlockSize(size_t size) noexcept override;
	void GetPreallocationStatus(size_t* block_size, size_t* last_allocated_block) noexcept override;
	void _truncate(const size_t &size);
	void _allocate(const size_t &offset, const size_t &length);
	void PrepareWrite(size_t offset, size_t len) noexcept override;
	Status Allocate(uint64_t offset, uint64_t len) noexcept override;
	Status PositionedAppend(const Slice& data, uint64_t offset) noexcept override;
	Status Append(const Slice& data) noexcept override;
	Status InvalidateCache(size_t offset, size_t length) noexcept override;
	Status Truncate(uint64_t size) noexcept override;
	Status RangeSync(uint64_t offset, uint64_t nbytes) noexcept override;
	Status Fsync() noexcept override;
	Status Sync() noexcept override;
	Status Flush() noexcept override;
	Status Close() noexcept override;

	writable_file(database *const &d, const std::string &name, const EnvOptions &, const bool &trunc);
	writable_file(const writable_file &) = delete;
	writable_file(writable_file &&) = delete;
	~writable_file() noexcept;
};
