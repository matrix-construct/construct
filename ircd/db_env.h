// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

/// Internal environment hookup.
///
struct [[gnu::visibility("hidden")]]
ircd::db::database::env final
:rocksdb::Env
{
	struct writable_file;
	struct writable_file_direct;
	struct sequential_file;
	struct random_access_file;
	struct random_rw_file;
	struct directory;
	struct file_lock;
	struct state;

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

	static int8_t make_nice(const IOPriority &);
	static int8_t make_nice(const Priority &);

	static ircd::log::log log;

	database &d;
	Env &defaults
	{
		*rocksdb::Env::Default()
	};

	std::unique_ptr<struct state> st;

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
	uint64_t NowNanos() noexcept override;
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

struct [[gnu::visibility("hidden")]]
ircd::db::database::env::directory final
:rocksdb::Directory
{
	using Status = rocksdb::Status;

	database &d;
	std::unique_ptr<Directory> defaults;

	Status Fsync() noexcept override;

	directory(database *const &d, const std::string &name, std::unique_ptr<Directory> defaults);
	~directory() noexcept;
};

struct [[gnu::visibility("hidden")]]
ircd::db::database::env::file_lock final
:rocksdb::FileLock
{
	database &d;

	file_lock(database *const &d);
	~file_lock() noexcept;
};

struct [[gnu::visibility("hidden")]]
ircd::db::database::env::random_access_file final
:rocksdb::RandomAccessFile
{
	using Status = rocksdb::Status;
	using Slice = rocksdb::Slice;

	static const fs::fd::opts default_opts;

	database &d;
	fs::fd::opts opts;
	fs::fd fd;
	size_t _buffer_align;
	int8_t ionice {0};
	bool aio;

	bool use_direct_io() const noexcept override;
	size_t GetRequiredBufferAlignment() const noexcept override;
	size_t GetUniqueId(char* id, size_t max_size) const noexcept override;
	void Hint(AccessPattern pattern) noexcept override;
	Status InvalidateCache(size_t offset, size_t length) noexcept override;
	Status Read(uint64_t offset, size_t n, Slice *result, char *scratch) const noexcept override;
	#ifdef IRCD_DB_HAS_ENV_MULTIREAD
	Status MultiRead(rocksdb::ReadRequest *, size_t num) noexcept override;
	#endif
	Status Prefetch(uint64_t offset, size_t n) noexcept override;

	random_access_file(database *const &d, const std::string &name, const EnvOptions &);
	~random_access_file() noexcept;
};

struct [[gnu::visibility("hidden")]]
ircd::db::database::env::random_rw_file final
:rocksdb::RandomRWFile
{
	using Status = rocksdb::Status;
	using Slice = rocksdb::Slice;

	static const fs::fd::opts default_opts;

	database &d;
	fs::fd::opts opts;
	fs::fd fd;
	size_t _buffer_align;
	int8_t ionice {0};
	bool nodelay {false};
	bool aio;

	bool use_direct_io() const noexcept override;
	size_t GetRequiredBufferAlignment() const noexcept override;
	Status Read(uint64_t offset, size_t n, Slice *result, char *scratch) const noexcept override;
	Status Write(uint64_t offset, const Slice &data) noexcept override;
	Status Flush() noexcept override;
	Status Sync() noexcept override;
	Status Fsync() noexcept override;
	Status Close() noexcept override;

	random_rw_file(database *const &d, const std::string &name, const EnvOptions &);
	~random_rw_file() noexcept;
};

struct [[gnu::visibility("hidden")]]
ircd::db::database::env::sequential_file final
:rocksdb::SequentialFile
{
	using Status = rocksdb::Status;
	using Slice = rocksdb::Slice;

	static const fs::fd::opts default_opts;

	database &d;
	ctx::mutex mutex;
	fs::fd::opts opts;
	fs::fd fd;
	size_t _buffer_align;
	off_t offset {0};
	int8_t ionice {0};
	bool aio;

	bool use_direct_io() const noexcept override;
	size_t GetRequiredBufferAlignment() const noexcept override;
	Status InvalidateCache(size_t offset, size_t length) noexcept override;
	Status PositionedRead(uint64_t offset, size_t n, Slice *result, char *scratch) noexcept override;
	Status Read(size_t n, Slice *result, char *scratch) noexcept override;
	Status Skip(uint64_t size) noexcept override;

	sequential_file(database *const &d, const std::string &name, const EnvOptions &);
	~sequential_file() noexcept;
};

struct [[gnu::visibility("hidden")]]
ircd::db::database::env::writable_file
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
	int8_t ionice {0};
	bool nodelay {false};
	WriteLifeTimeHint hint {WriteLifeTimeHint::WLTH_NOT_SET};
	fs::fd fd;
	size_t preallocation_block_size {0};
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

struct [[gnu::visibility("hidden")]]
ircd::db::database::env::writable_file_direct final
:writable_file
{
	size_t alignment {0};
	size_t logical_offset {0};
	unique_buffer<mutable_buffer> buffer;

	bool aligned(const size_t &) const;
	bool aligned(const void *const &) const;
	bool aligned(const const_buffer &) const;

	size_t align(const size_t &) const;
	size_t remain(const size_t &) const;
	size_t blocks(const size_t &) const;

	size_t buffer_remain() const;
	size_t buffer_consumed() const;

	const_buffer _write__aligned(const const_buffer &, const uint64_t &offset);
	const_buffer _write_aligned(const const_buffer &, const uint64_t &offset);
	const_buffer write_aligned(const const_buffer &);
	const_buffer write_unaligned_off(const const_buffer &);
	const_buffer write_unaligned_buf(const const_buffer &);
	const_buffer write(const const_buffer &);

	uint64_t GetFileSize() noexcept override;
	Status PositionedAppend(const Slice& data, uint64_t offset) noexcept override;
	Status Append(const Slice& data) noexcept override;
	Status Truncate(uint64_t size) noexcept override;
	Status Close() noexcept override;

	writable_file_direct(database *const &d, const std::string &name, const EnvOptions &, const bool &trunc);
};
