//
// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
// IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
// INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
// IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#pragma once
#define HAVE_IRCD_DB_DATABASE_ENV_H

// This file is not part of the standard include stack because it requires
// RocksDB symbols which we cannot forward declare. It is used internally
// and does not need to be included by general users of IRCd.

/// Internal environment hookup.
///
struct ircd::db::database::env final
:rocksdb::Env
{
	struct writable_file;
	struct sequential_file;
	struct random_access_file;
	struct random_rw_file;
	struct directory;
	struct file_lock;

	using Status = rocksdb::Status;
	using EnvOptions = rocksdb::EnvOptions;
	using Directory = rocksdb::Directory;
	using FileLock = rocksdb::FileLock;
	using WritableFile = rocksdb::WritableFile;
	using SequentialFile = rocksdb::SequentialFile;
	using RandomAccessFile = rocksdb::RandomAccessFile;
	using RandomRWFile = rocksdb::RandomRWFile;
	using Logger = rocksdb::Logger;
	using ThreadStatus = rocksdb::ThreadStatus;
	using ThreadStatusUpdater = rocksdb::ThreadStatusUpdater;

	database &d;
	Env &defaults
	{
		*rocksdb::Env::Default()
	};

	Status NewSequentialFile(const std::string& f, std::unique_ptr<SequentialFile>* r, const EnvOptions& options) noexcept override;
	Status NewRandomAccessFile(const std::string& f, std::unique_ptr<RandomAccessFile>* r, const EnvOptions& options) noexcept override;
	Status NewWritableFile(const std::string& f, std::unique_ptr<WritableFile>* r, const EnvOptions& options) noexcept override;
	Status ReopenWritableFile(const std::string& fname, std::unique_ptr<WritableFile>* result, const EnvOptions& options) noexcept override;
	Status ReuseWritableFile(const std::string& fname, const std::string& old_fname, std::unique_ptr<WritableFile>* r, const EnvOptions& options) noexcept override;
	Status NewRandomRWFile(const std::string& fname, std::unique_ptr<RandomRWFile>* result, const EnvOptions& options) noexcept override;
	Status NewDirectory(const std::string& name, std::unique_ptr<Directory>* result) noexcept override;
	Status FileExists(const std::string& f) noexcept override;
	Status GetChildren(const std::string& dir, std::vector<std::string>* r) noexcept override;
	Status GetChildrenFileAttributes(const std::string& dir, std::vector<FileAttributes>* result) noexcept override;
	Status DeleteFile(const std::string& f) noexcept override;
	Status CreateDir(const std::string& d) noexcept override;
	Status CreateDirIfMissing(const std::string& d) noexcept override;
	Status DeleteDir(const std::string& d) noexcept override;
	Status GetFileSize(const std::string& f, uint64_t* s) noexcept override;
	Status GetFileModificationTime(const std::string& fname, uint64_t* file_mtime) noexcept override;
	Status RenameFile(const std::string& s, const std::string& t) noexcept override;
	Status LinkFile(const std::string& s, const std::string& t) noexcept override;
	Status LockFile(const std::string& f, FileLock** l) noexcept override;
	Status UnlockFile(FileLock* l) noexcept override;
	void Schedule(void (*f)(void* arg), void* a, Priority pri, void* tag = nullptr, void (*u)(void* arg) = 0) noexcept override;
	int UnSchedule(void* tag, Priority pri) noexcept override;
	void StartThread(void (*f)(void*), void* a) noexcept override;
	void WaitForJoin() noexcept override;
	unsigned int GetThreadPoolQueueLen(Priority pri = LOW) const noexcept override;
	Status GetTestDirectory(std::string* path) noexcept override;
	Status NewLogger(const std::string& fname, std::shared_ptr<Logger>* result) noexcept override;
	uint64_t NowMicros() noexcept override;
	void SleepForMicroseconds(int micros) noexcept override;
	Status GetHostName(char* name, uint64_t len) noexcept override;
	Status GetCurrentTime(int64_t* unix_time) noexcept override;
	Status GetAbsolutePath(const std::string& db_path, std::string* output_path) noexcept override;
	void SetBackgroundThreads(int num, Priority pri) noexcept override;
	void IncBackgroundThreadsIfNeeded(int num, Priority pri) noexcept override;
	void LowerThreadPoolIOPriority(Priority pool = LOW) noexcept override;
	std::string TimeToString(uint64_t time) noexcept override;
	Status GetThreadList(std::vector<ThreadStatus>* thread_list) noexcept override;
	ThreadStatusUpdater* GetThreadStatusUpdater() const noexcept override;
	uint64_t GetThreadID() const noexcept override;

	env(database *const &d);
	~env() noexcept;
};

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

struct ircd::db::database::env::sequential_file final
:rocksdb::SequentialFile
{
	using Status = rocksdb::Status;
	using Slice = rocksdb::Slice;

	database &d;
	std::unique_ptr<SequentialFile> defaults;

	bool use_direct_io() const noexcept override;
	size_t GetRequiredBufferAlignment() const noexcept override;
	Status InvalidateCache(size_t offset, size_t length) noexcept override;
	Status PositionedRead(uint64_t offset, size_t n, Slice *result, char *scratch) noexcept override;
	Status Read(size_t n, Slice *result, char *scratch) noexcept override;
	Status Skip(uint64_t size) noexcept override;

	sequential_file(database *const &d, const std::string &name, const EnvOptions &, std::unique_ptr<SequentialFile> defaults);
	~sequential_file() noexcept;
};

struct ircd::db::database::env::random_access_file final
:rocksdb::RandomAccessFile
{
	using Status = rocksdb::Status;
	using Slice = rocksdb::Slice;

	database &d;
	std::unique_ptr<RandomAccessFile> defaults;

	bool use_direct_io() const noexcept override;
	size_t GetRequiredBufferAlignment() const noexcept override;
	size_t GetUniqueId(char* id, size_t max_size) const noexcept override;
	void Hint(AccessPattern pattern) noexcept override;
	Status InvalidateCache(size_t offset, size_t length) noexcept override;
	Status Read(uint64_t offset, size_t n, Slice *result, char *scratch) const noexcept override;
	Status Prefetch(uint64_t offset, size_t n) noexcept override;

	random_access_file(database *const &d, const std::string &name, const EnvOptions &, std::unique_ptr<RandomAccessFile> defaults);
	~random_access_file() noexcept;
};

struct ircd::db::database::env::random_rw_file final
:rocksdb::RandomRWFile
{
	using Status = rocksdb::Status;
	using Slice = rocksdb::Slice;

	database &d;
	std::unique_ptr<RandomRWFile> defaults;

	bool use_direct_io() const noexcept override;
	size_t GetRequiredBufferAlignment() const noexcept override;
	Status Read(uint64_t offset, size_t n, Slice *result, char *scratch) const noexcept override;
	Status Write(uint64_t offset, const Slice &data) noexcept override;
	Status Flush() noexcept override;
	Status Sync() noexcept override;
	Status Fsync() noexcept override;
	Status Close() noexcept override;

	random_rw_file(database *const &d, const std::string &name, const EnvOptions &, std::unique_ptr<RandomRWFile> defaults);
	~random_rw_file() noexcept;
};

struct ircd::db::database::env::directory final
:rocksdb::Directory
{
	using Status = rocksdb::Status;

	database &d;
	std::unique_ptr<Directory> defaults;

	Status Fsync() noexcept override;

	directory(database *const &d, const std::string &name, std::unique_ptr<Directory> defaults);
	~directory() noexcept;
};

struct ircd::db::database::env::file_lock final
:rocksdb::FileLock
{
	database &d;

	file_lock(database *const &d);
	~file_lock() noexcept;
};
