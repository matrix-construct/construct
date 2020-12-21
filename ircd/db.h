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

/// Uncomment or -D this #define to enable extensive log messages covering the
/// entire RocksDB callback surface. This is only useful for developers
/// specifically working on the backend of the DB and no real use for
/// developers making frontend queries to it. Massively verbose.
///
//#define RB_DEBUG_DB_ENV

/// This #define is more useful to developers making queries to the database.
/// It is still so verbose that it goes beyond what is tolerable and generally
/// useful even in debug-mode builds, thus the manual #define being required.
///
//#define RB_DEBUG_DB_SEEK
//#define RB_DEBUG_DB_SEEK_ROW
//#define IRCD_DB_DEBUG_PREFETCH

/// Uncomment or -D this #define to enable extensive log messages for the
/// experimental db environment-port implementation. This is only useful
/// for developers working on the port impl and want to debug all locking
/// and unlocking etc.
///
//#define RB_DEBUG_DB_PORT

#include <rocksdb/version.h>
#include <rocksdb/status.h>
#include <rocksdb/db.h>
#include <rocksdb/cache.h>
#include <rocksdb/comparator.h>
#include <rocksdb/merge_operator.h>
#include <rocksdb/perf_level.h>
#include <rocksdb/perf_context.h>
#include <rocksdb/iostats_context.h>
#include <rocksdb/listener.h>
#include <rocksdb/statistics.h>
#include <rocksdb/convenience.h>
#include <rocksdb/env.h>
#include <rocksdb/slice_transform.h>
#include <rocksdb/utilities/checkpoint.h>
#include <rocksdb/filter_policy.h>
#include <rocksdb/table.h>
#include <rocksdb/sst_file_manager.h>
#include <rocksdb/sst_dump_tool.h>
#include <rocksdb/compaction_filter.h>
#include <rocksdb/wal_filter.h>
#include <rocksdb/rate_limiter.h>

#include "db_has.h"
#include "db_port.h"
#include "db_env.h"
#include "db_env_state.h"

#pragma GCC visibility push(hidden)
namespace ircd::db
{
	struct throw_on_error;
	struct error_to_status;

	constexpr auto BLOCKING      { rocksdb::ReadTier::kReadAllTier      };
	constexpr auto NON_BLOCKING  { rocksdb::ReadTier::kBlockCacheTier   };

	// state
	extern log::log rog;
	extern conf::item<bool> enable_wal;
	extern conf::item<bool> read_checksum;
	extern conf::item<size_t> request_pool_size;
	extern conf::item<size_t> request_pool_stack_size;
	extern ctx::pool::opts request_pool_opts;
	extern ctx::pool request;

	// reflections
	string_view reflect(const rocksdb::Status::Code &);
	string_view reflect(const rocksdb::Status::Severity &);
	string_view reflect(const rocksdb::Env::Priority &p);
	string_view reflect(const rocksdb::Env::IOPriority &p);
	string_view reflect(const rocksdb::Env::WriteLifeTimeHint &);
	string_view reflect(const rocksdb::WriteStallCondition &);
	string_view reflect(const rocksdb::BackgroundErrorReason &);
	string_view reflect(const rocksdb::CompactionReason &);
	string_view reflect(const rocksdb::FlushReason &);
	string_view reflect(const rocksdb::RandomAccessFile::AccessPattern &p);
	const std::string &reflect(const rocksdb::Tickers &);
	const std::string &reflect(const rocksdb::Histograms &);

	// Frequently used get options and set options are separate from the string/map system
	rocksdb::WriteOptions make_opts(const sopts &);
	rocksdb::ReadOptions make_opts(const gopts &);

	// Database options creator
	static bool optstr_find_and_remove(std::string &optstr, const std::string &what);
	rocksdb::DBOptions make_dbopts(std::string optstr, std::string *const &out = nullptr, bool *read_only = nullptr, bool *fsck = nullptr);
	rocksdb::CompressionType find_supported_compression(const std::string &);

	// Read column names from filesystem
	std::vector<std::string> column_names(const std::string &path, const rocksdb::DBOptions &);
	std::vector<std::string> column_names(const std::string &path, const std::string &options);

	// Validation functors
	static bool valid(const rocksdb::Status &);
	using valid_proffer = std::function<bool (const rocksdb::Iterator &)>;
	static bool valid(const rocksdb::Iterator &, const valid_proffer &);
	static bool valid_eq(const rocksdb::Iterator &, const string_view &);
	static bool valid_lte(const rocksdb::Iterator &, const string_view &);
	static bool valid_gt(const rocksdb::Iterator &, const string_view &);
	static void valid_or_throw(const rocksdb::Iterator &);
	static void valid_eq_or_throw(const rocksdb::Iterator &, const string_view &);

	// [GET] iterator seek suite
	template<class pos> static bool seek(database::column &, const pos &, const rocksdb::ReadOptions &, std::unique_ptr<rocksdb::Iterator> &it);
	static std::unique_ptr<rocksdb::Iterator> seek(column &, const gopts &);
	static std::unique_ptr<rocksdb::Iterator> seek(column &, const string_view &key, const gopts &);
	static std::vector<row::value_type> seek(database &, const gopts &);
	static std::pair<string_view, string_view> operator*(const rocksdb::Iterator &);

	// [GET] read suite
	using _read_op = std::tuple<column, string_view>;
	using _read_closure = std::function<bool (column &, const column::delta &, const rocksdb::Status &)>;
	static bool _read(const vector_view<_read_op> &, const rocksdb::ReadOptions &, const _read_closure & = {});
	static rocksdb::Status _read(column &, const string_view &key, const rocksdb::ReadOptions &, const column::view_closure & = {});

	// [SET] writebatch suite
	string_view debug(const mutable_buffer &, const rocksdb::WriteBatch &);
	bool has(const rocksdb::WriteBatch &, const op &);
	void commit(database &, rocksdb::WriteBatch &, const rocksdb::WriteOptions &);
	void commit(database &, rocksdb::WriteBatch &, const sopts &);
	void append(rocksdb::WriteBatch &, column &, const column::delta &delta);
	void append(rocksdb::WriteBatch &, const cell::delta &delta);

	const descriptor &describe(const database::column &);
	const std::string &name(const database::column &);
	uint32_t id(const database::column &);

	bool dropped(const database::column &);
	void drop(database::column &);                   // Request to erase column from db

	std::shared_ptr<const database::column> shared_from(const database::column &);
	std::shared_ptr<database::column> shared_from(database::column &);
}
#pragma GCC visibility pop

#ifdef IRCD_DB_HAS_ALLOCATOR
/// Dynamic memory
struct [[gnu::visibility("hidden")]]
ircd::db::database::allocator final
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

struct [[gnu::visibility("hidden")]]
ircd::db::database::cache final
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

struct [[gnu::visibility("hidden")]]
ircd::db::database::comparator final
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

struct [[gnu::visibility("hidden")]]
ircd::db::database::prefix_transform final
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

struct [[gnu::visibility("hidden")]]
ircd::db::database::mergeop final
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

struct [[gnu::visibility("hidden")]]
ircd::db::database::compaction_filter final
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

struct [[gnu::visibility("hidden")]]
ircd::db::database::stats final
:rocksdb::Statistics
{
	static constexpr auto NUM_TICKER { rocksdb::TICKER_ENUM_MAX };
	static constexpr auto NUM_HISTOGRAM { rocksdb::HISTOGRAM_ENUM_MAX };

	struct passthru;

	database *d {nullptr};
	database::column *c {nullptr};
	std::array<uint64_t, NUM_TICKER> ticker {{0}};
	std::array<ircd::stats::item<uint64_t *>, NUM_TICKER> item;
	std::array<struct db::histogram, NUM_HISTOGRAM> histogram;

	// Additional custom stats
	ircd::stats::item<uint64_t> get_copied;
	ircd::stats::item<uint64_t> get_referenced;
	ircd::stats::item<uint64_t> multiget_copied;
	ircd::stats::item<uint64_t> multiget_referenced;

	string_view make_name(const string_view &ticker_name) const; // tls buffer

	uint64_t getTickerCount(const uint32_t tickerType) const noexcept override;
	uint64_t getAndResetTickerCount(const uint32_t tickerType) noexcept override;
	void recordTick(const uint32_t tickerType, const uint64_t count) noexcept override;
	void setTickerCount(const uint32_t tickerType, const uint64_t count) noexcept override;

	void histogramData(const uint32_t type, rocksdb::HistogramData *) const noexcept override;
	void measureTime(const uint32_t histogramType, const uint64_t time) noexcept override;
	bool HistEnabledForType(const uint32_t type) const noexcept override;
	rocksdb::Status Reset() noexcept override;

	stats() = default;
	stats(database *const &d, database::column *const &c = nullptr);
	~stats() noexcept;
};

struct [[gnu::visibility("hidden")]]
ircd::db::database::stats::passthru final
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

struct [[gnu::visibility("hidden")]]
ircd::db::database::column final
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
	rocksdb::WriteStallCondition stall;
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

struct [[gnu::visibility("hidden")]]
ircd::db::txn::handler
:rocksdb::WriteBatch::Handler
{
	using Status = rocksdb::Status;
	using Slice = rocksdb::Slice;

	const database &d;
	std::function<bool (const delta &)> cb;
	bool _continue {true};

	Status callback(const delta &) noexcept;
	Status callback(const uint32_t &, const op &, const Slice &a, const Slice &b) noexcept;

	bool Continue() noexcept override;
	Status MarkRollback(const Slice &xid) noexcept override;
	Status MarkCommit(const Slice &xid) noexcept override;
	Status MarkEndPrepare(const Slice &xid) noexcept override;
	Status MarkBeginPrepare(bool = false) noexcept override;

	Status MergeCF(const uint32_t cfid, const Slice &, const Slice &) noexcept override;
	Status SingleDeleteCF(const uint32_t cfid, const Slice &) noexcept override;
	Status DeleteRangeCF(const uint32_t cfid, const Slice &, const Slice &) noexcept override;
	Status DeleteCF(const uint32_t cfid, const Slice &) noexcept override;
	Status PutCF(const uint32_t cfid, const Slice &, const Slice &) noexcept override;

	handler(const database &d,
	        std::function<bool (const delta &)> cb)
	:d{d}
	,cb{std::move(cb)}
	{}
};

/// Callback surface for iterating/recovering the write-ahead-log journal.
struct [[gnu::visibility("hidden")]]
ircd::db::database::wal_filter
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

struct [[gnu::visibility("hidden")]]
ircd::db::database::events final
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

struct [[gnu::visibility("hidden")]]
ircd::db::database::logger final
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

struct [[gnu::visibility("hidden")]]
ircd::db::database::rate_limiter
:std::enable_shared_from_this<struct database::rate_limiter>
,rocksdb::RateLimiter
{
	using Statistics = rocksdb::Statistics;
	using IOPriority = rocksdb::Env::IOPriority;

	database *d {nullptr}; struct
	{
		int64_t count {0}, bytes {0};
	}
	requests[IOPriority::IO_TOTAL];
	int64_t bytes_per_second {1_GiB};

	bool IsRateLimited(OpType) noexcept override;
	int64_t GetBytesPerSecond() const noexcept override;
	int64_t GetSingleBurstBytes() const noexcept override;
	int64_t GetTotalRequests(const IOPriority) const noexcept override;
	int64_t GetTotalBytesThrough(const IOPriority) const noexcept override;

	size_t RequestToken(size_t, size_t, IOPriority, Statistics *, OpType) noexcept override;
	void SetBytesPerSecond(int64_t) noexcept override;

	rate_limiter(database *const &);
	~rate_limiter() noexcept;
};

//
// util
//

struct [[gnu::visibility("hidden")]]
ircd::db::throw_on_error
{
	throw_on_error(const rocksdb::Status & = rocksdb::Status::OK());
};

struct [[gnu::visibility("hidden")]]
ircd::db::error_to_status
:rocksdb::Status
{
	error_to_status(const std::error_code &);
	error_to_status(const std::system_error &);
	error_to_status(const std::exception &);
};
