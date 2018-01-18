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

	Status NewSequentialFile(const std::string& f, std::unique_ptr<SequentialFile>* r, const EnvOptions& options) override;
	Status NewRandomAccessFile(const std::string& f, std::unique_ptr<RandomAccessFile>* r, const EnvOptions& options) override;
	Status NewWritableFile(const std::string& f, std::unique_ptr<WritableFile>* r, const EnvOptions& options) override;
	Status ReopenWritableFile(const std::string& fname, std::unique_ptr<WritableFile>* result, const EnvOptions& options) override;
	Status ReuseWritableFile(const std::string& fname, const std::string& old_fname, std::unique_ptr<WritableFile>* r, const EnvOptions& options) override;
	Status NewRandomRWFile(const std::string& fname, std::unique_ptr<RandomRWFile>* result, const EnvOptions& options) override;
	Status NewDirectory(const std::string& name, std::unique_ptr<Directory>* result) override;
	Status FileExists(const std::string& f) override;
	Status GetChildren(const std::string& dir, std::vector<std::string>* r) override;
	Status GetChildrenFileAttributes(const std::string& dir, std::vector<FileAttributes>* result) override;
	Status DeleteFile(const std::string& f) override;
	Status CreateDir(const std::string& d) override;
	Status CreateDirIfMissing(const std::string& d) override;
	Status DeleteDir(const std::string& d) override;
	Status GetFileSize(const std::string& f, uint64_t* s) override;
	Status GetFileModificationTime(const std::string& fname, uint64_t* file_mtime) override;
	Status RenameFile(const std::string& s, const std::string& t) override;
	Status LinkFile(const std::string& s, const std::string& t) override;
	Status LockFile(const std::string& f, FileLock** l) override;
	Status UnlockFile(FileLock* l) override;
	void Schedule(void (*f)(void* arg), void* a, Priority pri, void* tag = nullptr, void (*u)(void* arg) = 0) override;
	int UnSchedule(void* tag, Priority pri) override;
	void StartThread(void (*f)(void*), void* a) override;
	void WaitForJoin() override;
	unsigned int GetThreadPoolQueueLen(Priority pri = LOW) const override;
	Status GetTestDirectory(std::string* path) override;
	Status NewLogger(const std::string& fname, std::shared_ptr<Logger>* result) override;
	uint64_t NowMicros() override;
	void SleepForMicroseconds(int micros) override;
	Status GetHostName(char* name, uint64_t len) override;
	Status GetCurrentTime(int64_t* unix_time) override;
	Status GetAbsolutePath(const std::string& db_path, std::string* output_path) override;
	void SetBackgroundThreads(int num, Priority pri) override;
	void IncBackgroundThreadsIfNeeded(int num, Priority pri) override;
	void LowerThreadPoolIOPriority(Priority pool = LOW) override;
	std::string TimeToString(uint64_t time) override;
	Status GetThreadList(std::vector<ThreadStatus>* thread_list) override;
	ThreadStatusUpdater* GetThreadStatusUpdater() const override;
	uint64_t GetThreadID() const override;

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

	Status Append(const Slice& data) override;
	Status PositionedAppend(const Slice& data, uint64_t offset) override;
	Status Truncate(uint64_t size) override;
	Status Close() override;
	Status Flush() override;
	Status Sync() override;
	Status Fsync() override;
	bool IsSyncThreadSafe() const override;
	void SetIOPriority(IOPriority pri) override;
	IOPriority GetIOPriority() override;
	uint64_t GetFileSize() override;
	void GetPreallocationStatus(size_t* block_size, size_t* last_allocated_block) override;
	size_t GetUniqueId(char* id, size_t max_size) const override;
	Status InvalidateCache(size_t offset, size_t length) override;
	void SetPreallocationBlockSize(size_t size) override;
	void PrepareWrite(size_t offset, size_t len) override;
	Status Allocate(uint64_t offset, uint64_t len) override;
	Status RangeSync(uint64_t offset, uint64_t nbytes) override;

	writable_file(database *const &d, const std::string &name, const EnvOptions &, std::unique_ptr<WritableFile> defaults);
	~writable_file() noexcept;
};
