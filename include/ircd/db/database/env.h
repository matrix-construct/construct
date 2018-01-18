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
struct ircd::db::database::env
:rocksdb::Env
{
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

	Status NewSequentialFile(const std::string& f, std::unique_ptr<SequentialFile>* r, const EnvOptions& options) final override;
	Status NewRandomAccessFile(const std::string& f, std::unique_ptr<RandomAccessFile>* r, const EnvOptions& options) final override;
	Status NewWritableFile(const std::string& f, std::unique_ptr<WritableFile>* r, const EnvOptions& options) final override;
	Status ReopenWritableFile(const std::string& fname, std::unique_ptr<WritableFile>* result, const EnvOptions& options) final override;
	Status ReuseWritableFile(const std::string& fname, const std::string& old_fname, std::unique_ptr<WritableFile>* r, const EnvOptions& options) final override;
	Status NewRandomRWFile(const std::string& fname, std::unique_ptr<RandomRWFile>* result, const EnvOptions& options) final override;
	Status NewDirectory(const std::string& name, std::unique_ptr<Directory>* result) final override;
	Status FileExists(const std::string& f) final override;
	Status GetChildren(const std::string& dir, std::vector<std::string>* r) final override;
	Status GetChildrenFileAttributes(const std::string& dir, std::vector<FileAttributes>* result) final override;
	Status DeleteFile(const std::string& f) final override;
	Status CreateDir(const std::string& d) final override;
	Status CreateDirIfMissing(const std::string& d) final override;
	Status DeleteDir(const std::string& d) final override;
	Status GetFileSize(const std::string& f, uint64_t* s) final override;
	Status GetFileModificationTime(const std::string& fname, uint64_t* file_mtime) final override;
	Status RenameFile(const std::string& s, const std::string& t) final override;
	Status LinkFile(const std::string& s, const std::string& t) final override;
	Status LockFile(const std::string& f, FileLock** l) final override;
	Status UnlockFile(FileLock* l) final override;
	void Schedule(void (*f)(void* arg), void* a, Priority pri, void* tag = nullptr, void (*u)(void* arg) = 0) final override;
	int UnSchedule(void* tag, Priority pri) final override;
	void StartThread(void (*f)(void*), void* a) final override;
	void WaitForJoin() final override;
	unsigned int GetThreadPoolQueueLen(Priority pri = LOW) const final override;
	Status GetTestDirectory(std::string* path) final override;
	Status NewLogger(const std::string& fname, std::shared_ptr<Logger>* result) final override;
	uint64_t NowMicros() final override;
	void SleepForMicroseconds(int micros) final override;
	Status GetHostName(char* name, uint64_t len) final override;
	Status GetCurrentTime(int64_t* unix_time) final override;
	Status GetAbsolutePath(const std::string& db_path, std::string* output_path) final override;
	void SetBackgroundThreads(int num, Priority pri) final override;
	void IncBackgroundThreadsIfNeeded(int num, Priority pri) final override;
	void LowerThreadPoolIOPriority(Priority pool = LOW) final override;
	std::string TimeToString(uint64_t time) final override;
	Status GetThreadList(std::vector<ThreadStatus>* thread_list) final override;
	ThreadStatusUpdater* GetThreadStatusUpdater() const final override;
	uint64_t GetThreadID() const final override;

	env(database *const &d);
	~env() noexcept;
};
