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
	int GetBackgroundThreads(Priority pri) noexcept override;

	env(database *const &d);
	~env() noexcept;
};
