// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::db
{
	const descriptor &describe(const database::column &);
	const std::string &name(const database::column &);
	uint32_t id(const database::column &);

	bool dropped(const database::column &);
	void drop(database::column &);                   // Request to erase column from db

	std::shared_ptr<const database::column> shared_from(const database::column &);
	std::shared_ptr<database::column> shared_from(database::column &);
}

#if ROCKSDB_MAJOR > 6 \
|| (ROCKSDB_MAJOR == 6 && ROCKSDB_MINOR > 4) \
|| (ROCKSDB_MAJOR == 6 && ROCKSDB_MINOR == 4 && ROCKSDB_PATCH >= 6)
	#define IRCD_DB_HAS_CACHE_GETCHARGE
#endif

#if ROCKSDB_MAJOR > 5 \
|| (ROCKSDB_MAJOR == 5 && ROCKSDB_MINOR >= 18)
	#define IRCD_DB_HAS_ALLOCATOR
#endif

struct ircd::db::database::cache final
:rocksdb::Cache
{
	using Slice = rocksdb::Slice;
	using Status = rocksdb::Status;
	using deleter = void (*)(const Slice &key, void *value);
	using callback = void (*)(void *, size_t);
	using Statistics = rocksdb::Statistics;

	static const int DEFAULT_SHARD_BITS;
	static const double DEFAULT_HI_PRIO;
	static const bool DEFAULT_STRICT;

	database *d;
	std::string name;
	std::shared_ptr<struct database::stats> stats;
	std::shared_ptr<struct database::allocator> allocator;
	std::shared_ptr<rocksdb::Cache> c;

	const char *Name() const noexcept override;
	Status Insert(const Slice &key, void *value, size_t charge, deleter, Handle **, Priority) noexcept override;
	Handle *Lookup(const Slice &key, Statistics *) noexcept override;
	bool Ref(Handle *) noexcept override;
	bool Release(Handle *, bool force_erase) noexcept override;
	void *Value(Handle *) noexcept override;
	void Erase(const Slice &key) noexcept override;
	uint64_t NewId() noexcept override;
	void SetCapacity(size_t capacity) noexcept override;
	void SetStrictCapacityLimit(bool strict_capacity_limit) noexcept override;
	bool HasStrictCapacityLimit() const noexcept override;
	size_t GetCapacity() const noexcept override;
	size_t GetUsage() const noexcept override;
	size_t GetUsage(Handle *) const noexcept override;
	size_t GetPinnedUsage() const noexcept override;
	void DisownData() noexcept override;
	void ApplyToAllCacheEntries(callback, bool thread_safe) noexcept override;
	void EraseUnRefEntries() noexcept override;
	std::string GetPrintableOptions() const noexcept override;
	#ifdef IRCD_DB_HAS_CACHE_GETCHARGE
	size_t GetCharge(Handle *) const noexcept override;
	#endif

	cache(database *const &,
	      std::shared_ptr<struct database::stats>,
	      std::shared_ptr<struct database::allocator>,
	      std::string name,
	      const ssize_t &initial_capacity = -1);

	~cache() noexcept override;
};

struct ircd::db::database::comparator final
:rocksdb::Comparator
{
	using Slice = rocksdb::Slice;

	database *d;
	db::comparator user;

	bool CanKeysWithDifferentByteContentsBeEqual() const noexcept override;
	bool IsSameLengthImmediateSuccessor(const Slice &s, const Slice &t) const noexcept override;
	void FindShortestSeparator(std::string *start, const Slice &limit) const noexcept override;
	void FindShortSuccessor(std::string *key) const noexcept override;
	int Compare(const Slice &a, const Slice &b) const noexcept override;
	bool Equal(const Slice &a, const Slice &b) const noexcept override;
	const char *Name() const noexcept override;

	comparator(database *const &d, db::comparator user);
};

struct ircd::db::database::prefix_transform final
:rocksdb::SliceTransform
{
	using Slice = rocksdb::Slice;

	database *d;
	db::prefix_transform user;

	const char *Name() const noexcept override;
	bool InDomain(const Slice &key) const noexcept override;
	bool InRange(const Slice &key) const noexcept override;
	Slice Transform(const Slice &key) const noexcept override;

	prefix_transform(database *const &d,
	                 db::prefix_transform user)
	noexcept
	:d{d}
	,user{std::move(user)}
	{}
};

struct ircd::db::database::mergeop final
:std::enable_shared_from_this<struct ircd::db::database::mergeop>
,rocksdb::AssociativeMergeOperator
{
	database *d;
	merge_closure merger;

	bool Merge(const rocksdb::Slice &, const rocksdb::Slice *, const rocksdb::Slice &, std::string *, rocksdb::Logger *) const noexcept override;
	const char *Name() const noexcept override;

	mergeop(database *const &d, merge_closure merger = nullptr);
	~mergeop() noexcept;
};

struct ircd::db::database::compaction_filter final
:rocksdb::CompactionFilter
{
	using Slice = rocksdb::Slice;

	column *c;
	database *d;
	db::compactor user;

	const char *Name() const noexcept override;
	bool IgnoreSnapshots() const noexcept override;
	Decision FilterV2(const int level, const Slice &key, const ValueType v, const Slice &oldval, std::string *newval, std::string *skipuntil) const noexcept override;

	compaction_filter(column *const &c, db::compactor);
	~compaction_filter() noexcept override;
};

struct ircd::db::database::column final
:std::enable_shared_from_this<database::column>
,rocksdb::ColumnFamilyDescriptor
{
	database *d;
	db::descriptor *descriptor;
	std::type_index key_type;
	std::type_index mapped_type;
	comparator cmp;
	prefix_transform prefix;
	compaction_filter cfilter;
	std::shared_ptr<struct database::stats> stats;
	std::shared_ptr<struct database::allocator> allocator;
	rocksdb::BlockBasedTableOptions table_opts;
	custom_ptr<rocksdb::ColumnFamilyHandle> handle;

  public:
	operator const rocksdb::ColumnFamilyOptions &() const;
	operator const rocksdb::ColumnFamilyHandle *() const;
	operator const database &() const;

	operator rocksdb::ColumnFamilyHandle *();
	operator database &();

	explicit column(database &d, db::descriptor &);
	column() = delete;
	column(column &&) = delete;
	column(const column &) = delete;
	column &operator=(column &&) = delete;
	column &operator=(const column &) = delete;
	~column() noexcept;
};

struct ircd::db::database::events final
:std::enable_shared_from_this<struct ircd::db::database::events>
,rocksdb::EventListener
{
	database *d;

	void OnFlushBegin(rocksdb::DB *, const rocksdb::FlushJobInfo &) noexcept override;
	void OnFlushCompleted(rocksdb::DB *, const rocksdb::FlushJobInfo &) noexcept override;
	void OnCompactionCompleted(rocksdb::DB *, const rocksdb::CompactionJobInfo &) noexcept override;
	void OnTableFileDeleted(const rocksdb::TableFileDeletionInfo &) noexcept override;
	void OnTableFileCreated(const rocksdb::TableFileCreationInfo &) noexcept override;
	void OnTableFileCreationStarted(const rocksdb::TableFileCreationBriefInfo &) noexcept override;
	void OnMemTableSealed(const rocksdb::MemTableInfo &) noexcept override;
	void OnColumnFamilyHandleDeletionStarted(rocksdb::ColumnFamilyHandle *) noexcept override;
	void OnExternalFileIngested(rocksdb::DB *, const rocksdb::ExternalFileIngestionInfo &) noexcept override;
	void OnBackgroundError(rocksdb::BackgroundErrorReason, rocksdb::Status *) noexcept override;
	void OnStallConditionsChanged(const rocksdb::WriteStallInfo &) noexcept override;

	events(database *const &d)
	noexcept
	:d{d}
	{}
};

struct ircd::db::database::logger final
:std::enable_shared_from_this<struct database::logger>
,rocksdb::Logger
{
	database *d;

	void Logv(const rocksdb::InfoLogLevel level, const char *fmt, va_list ap) noexcept override;
	void Logv(const char *fmt, va_list ap) noexcept override;
	void LogHeader(const char *fmt, va_list ap) noexcept override;

	rocksdb::Status Close() noexcept override;

	logger(database *const &d);
	~logger() noexcept override;
};

struct ircd::db::database::stats final
:rocksdb::Statistics
{
	struct passthru;

	database *d {nullptr};
	std::array<uint64_t, rocksdb::TICKER_ENUM_MAX> ticker {{0}};
	std::array<struct db::histogram, rocksdb::HISTOGRAM_ENUM_MAX> histogram;

	uint64_t getTickerCount(const uint32_t tickerType) const noexcept override;
	void recordTick(const uint32_t tickerType, const uint64_t count) noexcept override;
	void setTickerCount(const uint32_t tickerType, const uint64_t count) noexcept override;
	void histogramData(const uint32_t type, rocksdb::HistogramData *) const noexcept override;
	void measureTime(const uint32_t histogramType, const uint64_t time) noexcept override;
	bool HistEnabledForType(const uint32_t type) const noexcept override;
	uint64_t getAndResetTickerCount(const uint32_t tickerType) noexcept override;
	rocksdb::Status Reset() noexcept override;

	stats(database *const &d = nullptr);
	~stats() noexcept;
};

struct ircd::db::database::stats::passthru final
:rocksdb::Statistics
{
	std::array<rocksdb::Statistics *, 2> pass {{nullptr}};

	void recordTick(const uint32_t tickerType, const uint64_t count) noexcept override;
	void measureTime(const uint32_t histogramType, const uint64_t time) noexcept override;
	bool HistEnabledForType(const uint32_t type) const noexcept override;
	uint64_t getTickerCount(const uint32_t tickerType) const noexcept override;
	void setTickerCount(const uint32_t tickerType, const uint64_t count) noexcept override;
	void histogramData(const uint32_t type, rocksdb::HistogramData *) const noexcept override;
	uint64_t getAndResetTickerCount(const uint32_t tickerType) noexcept override;
	rocksdb::Status Reset() noexcept override;

	passthru(rocksdb::Statistics *const &a, rocksdb::Statistics *const &b);
	~passthru() noexcept;
};

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

#ifdef IRCD_DB_HAS_ALLOCATOR
/// Dynamic memory
struct ircd::db::database::allocator final
:rocksdb::MemoryAllocator
{
	static const size_t ALIGN_DEFAULT;
	static const size_t mlock_limit;
	static const bool mlock_enabled;
	static size_t mlock_current;
	static unsigned cache_arena;

	database *d {nullptr};
	database::column *c {nullptr};
	size_t alignment {ALIGN_DEFAULT};
	unsigned arena {0};
	signed arena_flags{0};

	const char *Name() const noexcept override;
	void *Allocate(size_t) noexcept override;
	void Deallocate(void *) noexcept override;
	size_t UsableSize(void *, size_t) const noexcept override;

	allocator(database *const &,
	          database::column *const &  = nullptr,
	          const unsigned &arena      = 0,
	          const size_t &alignment    = ALIGN_DEFAULT);

	~allocator() noexcept;

	static void init(), fini() noexcept;
};
#endif
