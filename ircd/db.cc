// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

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

// ircd::db interfaces requiring complete RocksDB (frontside).
#include <ircd/db/database/comparator.h>
#include <ircd/db/database/prefix_transform.h>
#include <ircd/db/database/mergeop.h>
#include <ircd/db/database/events.h>
#include <ircd/db/database/stats.h>
#include <ircd/db/database/logs.h>
#include <ircd/db/database/column.h>
#include <ircd/db/database/txn.h>

// RocksDB embedding environment callback interfaces (backside).
#include <ircd/db/database/env/env.h>
#include <ircd/db/database/env/writable_file.h>
#include <ircd/db/database/env/sequential_file.h>
#include <ircd/db/database/env/random_access_file.h>
#include <ircd/db/database/env/random_rw_file.h>
#include <ircd/db/database/env/directory.h>
#include <ircd/db/database/env/file_lock.h>
#include <ircd/db/database/env/state.h>

#define IRCD_DB_PORT

// RocksDB port linktime-overriding interfaces (experimental).
#ifdef IRCD_DB_PORT
#include <ircd/db/database/env/port.h>
#endif

// Internal utility interface for this definition file.
#include "db.h"

//
// Misc / General linkages
//

/// Dedicated logging facility for the database subsystem
decltype(ircd::db::log)
ircd::db::log
{
	"db", 'D'
};

/// Dedicated logging facility for rocksdb's log callbacks
decltype(ircd::db::rog)
ircd::db::rog
{
	"rdb", 'R'
};

ircd::conf::item<size_t>
ircd::db::request_pool_stack_size
{
	{ "name",     "ircd.db.request_pool.stack_size" },
	{ "default",  long(128_KiB)                     },
};

ircd::conf::item<size_t>
ircd::db::request_pool_size
{
	{
		{ "name",     "ircd.db.request_pool.size" },
		{ "default",  32L                         },
	}, []
	{
		request.set(size_t(request_pool_size));
	}
};

/// Concurrent request pool. Requests to seek may be executed on this
/// pool in cases where a single context would find it advantageous.
/// Some examples are a db::row seek, or asynchronous prefetching.
///
/// The number of workers in this pool should upper bound at the
/// number of concurrent AIO requests which are effective on this
/// system. This is a static pool shared by all databases.
decltype(ircd::db::request)
ircd::db::request
{
	"db req",
	size_t(request_pool_stack_size),
	0, // don't prespawn because this is static
};

decltype(ircd::db::write_mutex)
ircd::db::write_mutex;

///////////////////////////////////////////////////////////////////////////////
//
// init
//

namespace ircd::db
{
	static void init_compressions();
	static void init_directory();
	static void init_version();
}

static char ircd_db_version[64];
const char *const ircd::db::version(ircd_db_version);

ircd::db::init::init()
{
	init_compressions();
	init_directory();
	request.add(request_pool_size);
}

ircd::db::init::~init()
noexcept
{
	if(request.active())
		log::warning
		{
			log, "Terminating %zu active of %zu client request contexts; %zu pending; %zu queued",
			request.active(),
			request.size(),
			request.pending(),
			request.queued()
		};

	request.terminate();
	log::debug
	{
		log, "Waiting for %zu active of %zu client request contexts; %zu pending; %zu queued",
		request.active(),
		request.size(),
		request.pending(),
		request.queued()
	};

	request.join();
	log::debug
	{
		log, "All contexts joined; all requests are clear."
	};
}

void
ircd::db::init_directory()
try
{
	const auto dbdir
	{
		fs::get(fs::DB)
	};

	if(fs::mkdir(dbdir))
		log::notice
		{
			log, "Created new database directory at `%s'", dbdir
		};
	else
		log::info
		{
			log, "Using database directory at `%s'", dbdir
		};
}
catch(const fs::error &e)
{
	log::error
	{
		log, "Cannot start database system: %s", e.what()
	};

	if(ircd::debugmode)
		throw;
}

void
ircd::db::init_compressions()
{
	const auto compressions
	{
		rocksdb::GetSupportedCompressions()
	};

	if(compressions.empty())
		log::warning
		{
			"No compression libraries have been linked with the DB."
			" This is probably not what you want."
		};
}

// Renders a version string from the defines included here.
__attribute__((constructor))
void
ircd::db::init_version()
{
	snprintf(ircd_db_version, sizeof(ircd_db_version), "%d.%d.%d",
	         ROCKSDB_MAJOR,
	         ROCKSDB_MINOR,
	         ROCKSDB_PATCH);
}

///////////////////////////////////////////////////////////////////////////////
//
// database
//

void
ircd::db::sync(database &d)
{
	const ctx::uninterruptible ui;
	throw_on_error
	{
		d.d->SyncWAL()
	};
}

/// Flushes all columns. Note that if blocking=true, blocking may occur for
/// each column individually.
void
ircd::db::flush(database &d,
                const bool &sync)
{
	const ctx::uninterruptible ui;
	throw_on_error
	{
		d.d->FlushWAL(sync)
	};
}

/// Moves memory structures to SST files for all columns. This doesn't
/// necessarily sort anything that wasn't previously sorted, but it may create
/// new SST files and shouldn't be confused with a typical fflush().
/// Note that if blocking=true, blocking may occur for each column individually.
void
ircd::db::sort(database &d,
               const bool &blocking)
{
	for(const auto &c : d.columns)
	{
		db::column column{*c};
		db::sort(column, blocking);
	}
}

void
ircd::db::compact(database &d)
{
	static const std::pair<string_view, string_view> range
	{
		{}, {}
	};

	for(const auto &c : d.columns)
	{
		db::column column{*c};
		compact(column, range, -1);
		compact(column, -1);
	}
}

void
ircd::db::check(database &d)
{
	assert(d.d);
	const ctx::uninterruptible ui;
	throw_on_error
	{
		d.d->VerifyChecksum()
	};
}

/// Writes a snapshot of this database to the directory specified. The
/// snapshot consists of hardlinks to the bulk data files of this db, but
/// copies the other stuff that usually gets corrupted. The directory can
/// then be opened as its own database either read-only or read-write.
/// Incremental backups and rollbacks can begin from this interface. Note
/// this may be an expensive blocking operation.
uint64_t
ircd::db::checkpoint(database &d)
{
	const ctx::uninterruptible ui;
	if(!d.checkpointer)
		throw error
		{
			"Checkpointing is not available for db(%p) '%s",
			&d,
			name(d)
		};

	const auto seqnum
	{
		sequence(d)
	};

	const std::string dir
	{
		db::path(name(d), seqnum)
	};

	throw_on_error
	{
		d.checkpointer->CreateCheckpoint(dir, 0)
	};

	log::debug
	{
		log, "'%s': Checkpoint at sequence %lu in `%s' complete",
		name(d),
		seqnum,
		dir
	};

	return seqnum;
}

/// This wraps RocksDB's "File Deletions" which means after RocksDB
/// compresses some file it then destroys the uncompressed version;
/// setting this to false will disable that and retain both versions.
/// This is useful when a direct reference is being manually held by
/// us into the uncompressed version which must remain valid.
void
ircd::db::fdeletions(database &d,
                     const bool &enable,
                     const bool &force)
{
	const ctx::uninterruptible ui;
	if(enable) throw_on_error
	{
		d.d->EnableFileDeletions(force)
	};
	else throw_on_error
	{
		d.d->DisableFileDeletions()
	};
}

void
ircd::db::setopt(database &d,
                 const string_view &key,
                 const string_view &val)
{
	const ctx::uninterruptible ui;
	const std::unordered_map<std::string, std::string> options
	{
		{ std::string{key}, std::string{val} }
	};

	throw_on_error
	{
		d.d->SetDBOptions(options)
	};
}

uint64_t
ircd::db::ticker(const database &d,
                 const string_view &key)
{
	return ticker(d, ticker_id(key));
}

uint64_t
ircd::db::ticker(const database &d,
                 const uint32_t &id)
{
	return d.stats->getTickerCount(id);
}

size_t
ircd::db::bytes(const database &d)
{
	return std::accumulate(begin(d.columns), end(d.columns), size_t(0), []
	(auto ret, const auto &colptr)
	{
		db::column c{*colptr};
		return ret += db::bytes(c);
	});
}

size_t
ircd::db::file_count(const database &d)
{
	return std::accumulate(begin(d.columns), end(d.columns), size_t(0), []
	(auto ret, const auto &colptr)
	{
		db::column c{*colptr};
		return ret += db::file_count(c);
	});
}

/// Get the live file list for db; see overlord documentation.
std::vector<std::string>
ircd::db::files(const database &d)
{
	uint64_t ignored;
	return files(d, ignored);
}

/// Get the live file list for database relative to the database's directory.
/// One of the files is a manifest file which is over-allocated and its used
/// size is returned in the integer passed to the `msz` argument.
///
/// This list may not be completely up to date. The reliable way to get the
/// most current list is to flush all columns first and ensure no database
/// activity took place between the flushing and this query.
std::vector<std::string>
ircd::db::files(const database &cd,
                uint64_t &msz)
{
	const ctx::uninterruptible ui;
	std::vector<std::string> ret;
	auto &d(const_cast<database &>(cd));
	throw_on_error
	{
		d.d->GetLiveFiles(ret, &msz, false)
	};

	return ret;
}

uint64_t
ircd::db::sequence(const database &cd)
{
	database &d(const_cast<database &>(cd));
	return d.d->GetLatestSequenceNumber();
}

rocksdb::Cache *
ircd::db::cache(database &d)
{
	return d.cache.get();
}

const rocksdb::Cache *
ircd::db::cache(const database &d)
{
	return d.cache.get();
}

template<>
ircd::db::prop_int
ircd::db::property(const database &cd,
                   const string_view &name)
{
	uint64_t ret;
	database &d(const_cast<database &>(cd));
	if(!d.d->GetAggregatedIntProperty(slice(name), &ret))
		throw not_found
		{
			"property '%s' for all columns in '%s' not found or not an integer.",
			name,
			db::name(d)
		};

	return ret;
}

std::shared_ptr<ircd::db::database::column>
ircd::db::shared_from(database::column &column)
{
	return column.shared_from_this();
}

std::shared_ptr<const ircd::db::database::column>
ircd::db::shared_from(const database::column &column)
{
	return column.shared_from_this();
}

const std::string &
ircd::db::uuid(const database &d)
{
	return d.uuid;
}

const std::string &
ircd::db::name(const database &d)
{
	return d.name;
}

//
// database
//

namespace ircd::db
{
	extern const database::description default_description;
}

// Instance list linkage
template<>
decltype(ircd::util::instance_list<ircd::db::database>::list)
ircd::util::instance_list<ircd::db::database>::list
{};

decltype(ircd::db::default_description)
ircd::db::default_description
{
	/// Requirement of RocksDB going back to LevelDB. This column must
	/// always exist in all descriptions and probably should be at idx[0].
	{ "default" }
};

ircd::db::database &
ircd::db::database::get(column &column)
{
	assert(column.d);
	return *column.d;
}

const ircd::db::database &
ircd::db::database::get(const column &column)
{
	assert(column.d);
	return *column.d;
}

ircd::db::database &
ircd::db::database::get(const string_view &name)
{
	const auto pair
	{
		namepoint(name)
	};

	return get(pair.first, pair.second);
}

ircd::db::database &
ircd::db::database::get(const string_view &name,
                        const uint64_t &checkpoint)
{
	auto *const &d
	{
		get(std::nothrow, name, checkpoint)
	};

	if(likely(d))
		return *d;

	throw checkpoint == uint64_t(-1)?
		std::out_of_range{"No database with that name exists"}:
		std::out_of_range{"No database with that name at that checkpoint exists"};
}

ircd::db::database *
ircd::db::database::get(std::nothrow_t,
                        const string_view &name)
{
	const auto pair
	{
		namepoint(name)
	};

	return get(std::nothrow, pair.first, pair.second);
}

ircd::db::database *
ircd::db::database::get(std::nothrow_t,
                        const string_view &name,
                        const uint64_t &checkpoint)
{
	for(auto *const &d : list)
		if(name == d->name)
			if(checkpoint == uint64_t(-1) || checkpoint == d->checkpoint)
				return d;

	return nullptr;
}

//
// database::database
//

ircd::db::database::database(const string_view &name,
                             std::string optstr)
:database
{
	name, std::move(optstr), default_description
}
{
}

ircd::db::database::database(const string_view &name,
                             std::string optstr,
                             description description)
:database
{
	namepoint(name).first, namepoint(name).second, std::move(optstr), std::move(description)
}
{
}

ircd::db::database::database(const string_view &name,
                             const uint64_t &checkpoint,
                             std::string optstr,
                             description description)
try
:name
{
	namepoint(name).first
}
,checkpoint
{
	// a -1 may have been generated by the db::namepoint() util when the user
	// supplied just a name without a checkpoint. In the context of database
	// opening/creation -1 just defaults to 0.
	checkpoint == uint64_t(-1)? 0 : checkpoint
}
,optstr
{
	std::move(optstr)
}
,path
{
	db::path(this->name, this->checkpoint)
}
,env
{
	std::make_shared<struct env>(this)
}
,logs
{
	std::make_shared<struct logs>(this)
}
,stats
{
	std::make_shared<struct stats>(this)
}
,events
{
	std::make_shared<struct events>(this)
}
,mergeop
{
	std::make_shared<struct mergeop>(this)
}
,ssts
{
	// note: the sst file manager cannot be used for now because it will spawn
	// note: a pthread internally in rocksdb which does not use our callbacks
	// note: we gave in the supplied env. we really don't want that.

	//rocksdb::NewSstFileManager(env.get(), logs, {}, 0, true, nullptr, 0.05)
}
,cache{[this]
() -> std::shared_ptr<rocksdb::Cache>
{
	//TODO: conf
	const auto lru_cache_size{16_MiB};
	return rocksdb::NewLRUCache(lru_cache_size);
}()}
,descriptors
{
	std::move(description)
}
,column_names{[this]
{
	// Existing columns at path. If any are left the descriptor set did not
	// describe all of the columns found in the database at path.
	const auto opts
	{
		make_dbopts(this->optstr)
	};

	const auto required
	{
		db::column_names(path, opts)
	};

	std::set<string_view> existing
	{
		begin(required), end(required)
	};

	// The names of the columns extracted from the descriptor set
	std::vector<string_view> ret(descriptors.size());
	std::transform(begin(descriptors), end(descriptors), begin(ret), [&existing]
	(const auto &descriptor) -> string_view
	{
		existing.erase(descriptor.name);
		return descriptor.name;
	});

	for(const auto &remain : existing)
		throw error
		{
			"Failed to describe existing column '%s' (and %zd others...)",
			remain,
			existing.size() - 1
		};

	return ret;
}()}
,column_index{[this]
{
	decltype(this->column_index) ret;
	for(const auto &descriptor : this->descriptors)
		ret.emplace(descriptor.name, -1);

	return ret;
}()}
,d{[this]
{
	bool fsck{false};
	bool read_only{false};
	auto opts
	{
		make_dbopts(this->optstr, &this->optstr, &read_only, &fsck)
	};

	// Setup sundry
	opts.create_if_missing = true;
	opts.create_missing_column_families = true;
	opts.max_file_opening_threads = 0;
	opts.stats_dump_period_sec = 0;
	opts.enable_thread_tracking = true;
	opts.avoid_flush_during_recovery = true;
	opts.delete_obsolete_files_period_micros = 0;
	opts.max_background_jobs = 2;
	opts.max_background_flushes = 1;
	opts.max_background_compactions = 1;
	opts.max_subcompactions = 1;
	opts.max_open_files = -1; //ircd::info::rlimit_nofile / 4;
	opts.allow_concurrent_memtable_write = false;
	opts.enable_write_thread_adaptive_yield = false;
	opts.enable_pipelined_write = false;
	opts.use_direct_reads = true;
	opts.write_thread_max_yield_usec = 0;
	opts.write_thread_slow_yield_usec = 0;
	opts.use_direct_io_for_flush_and_compaction = false;

	#ifdef RB_DEBUG
	opts.dump_malloc_stats = true;
	#endif

	// Setup env
	opts.env = env.get();

	// Setup SST file mgmt
	opts.sst_file_manager = this->ssts;

	// Setup logging
	logs->SetInfoLogLevel(ircd::debugmode? rocksdb::DEBUG_LEVEL : rocksdb::WARN_LEVEL);
	opts.info_log_level = logs->GetInfoLogLevel();
	opts.info_log = logs;

	// Setup event and statistics callbacks
	opts.listeners.emplace_back(this->events);

	// Setup histogram collecting
	//this->stats->stats_level_ = rocksdb::kAll;
	this->stats->stats_level_ = rocksdb::kExceptTimeForMutex;
	opts.statistics = this->stats;

	// Setup performance metric options
	//rocksdb::SetPerfLevel(rocksdb::PerfLevel::kDisable);

	// Default corruption tolerance is zero-tolerance; db fails to open with
	// error by default to inform the user. The rest of the options are
	// various relaxations for how to proceed.
	opts.wal_recovery_mode = rocksdb::WALRecoveryMode::kAbsoluteConsistency;

	// When corrupted after crash, the DB is rolled back before the first
	// corruption and erases everything after it, giving a consistent
	// state up at that point, though losing some recent data.
	if(ircd::pitrecdb) //TODO: no global?
		opts.wal_recovery_mode = rocksdb::WALRecoveryMode::kPointInTimeRecovery;

	// Skipping corrupted records will create gaps in the DB timeline where the
	// application (like a matrix timeline) cannot tolerate the unexpected gap.
	//opts.wal_recovery_mode = rocksdb::WALRecoveryMode::kSkipAnyCorruptedRecords;

	// Tolerating corrupted records is very last-ditch for getting the database to
	// open in a catastrophe. We have no use for this option but should use it for
	//TODO: emergency salvage-mode.
	//opts.wal_recovery_mode = rocksdb::WALRecoveryMode::kTolerateCorruptedTailRecords;

	// Setup cache
	opts.row_cache = this->cache;

	// Setup column families
	for(const auto &desc : descriptors)
	{
		const auto c
		{
			std::make_shared<column>(this, desc)
		};

		columns.emplace_back(c);
	}

	std::vector<rocksdb::ColumnFamilyHandle *> handles;
	std::vector<rocksdb::ColumnFamilyDescriptor> columns(this->columns.size());
	std::transform(begin(this->columns), end(this->columns), begin(columns), []
	(const auto &column)
	{
		return static_cast<const rocksdb::ColumnFamilyDescriptor &>(*column);
	});

	// NOTE: rocksdb sez RepairDB is broken; can't use now
	if(fsck && fs::is_dir(path))
	{
		const ctx::uninterruptible ui;

		log::notice
		{
			log, "Checking database @ `%s' columns[%zu]", path, columns.size()
		};

		throw_on_error
		{
			rocksdb::RepairDB(path, opts, columns)
		};

		log::info
		{
			log, "Database @ `%s' check complete", path
		};
	}

	// If the directory does not exist, though rocksdb will create it, we can
	// avoid scaring the user with an error log message if we just do that..
	if(opts.create_if_missing && !fs::is_dir(path))
		fs::mkdir(path);

	// Announce attempt before usual point where exceptions are thrown
	const ctx::uninterruptible ui;
	log::info
	{
		log, "Opening database \"%s\" @ `%s' with %zu columns...",
		this->name,
		path,
		columns.size()
	};

	// Open DB into ptr
	rocksdb::DB *ptr;
	if(read_only)
		throw_on_error
		{
			rocksdb::DB::OpenForReadOnly(opts, path, columns, &handles, &ptr)
		};
	else
		throw_on_error
		{
			rocksdb::DB::Open(opts, path, columns, &handles, &ptr)
		};

	std::unique_ptr<rocksdb::DB> ret
	{
		ptr
	};

	for(const auto &handle : handles)
	{
		this->columns.at(handle->GetID())->handle.reset(handle);
		this->column_index.at(handle->GetName()) = handle->GetID();
	}

	for(size_t i(0); i < this->columns.size(); ++i)
		if(db::id(*this->columns[i]) != i)
			throw error
			{
				"Columns misaligned: expecting id[%zd] got id[%u] '%s'",
				i,
				db::id(*this->columns[i]),
				db::name(*this->columns[i])
			};

	return ret;
}()}
,uuid{[this]
{
	const ctx::uninterruptible ui;
	std::string ret;
	throw_on_error
	{
		d->GetDbIdentity(ret)
	};

	return ret;
}()}
,checkpointer{[this]
{
	const ctx::uninterruptible ui;
	rocksdb::Checkpoint *checkpointer{nullptr};
	throw_on_error
	{
		rocksdb::Checkpoint::Create(this->d.get(), &checkpointer)
	};

	return checkpointer;
}()}
{
	if(ircd::checkdb)
	{
		log::notice
		{
			log, "'%s': Verifying database integrity. This may take several minutes...",
			this->name
		};

		const ctx::uninterruptible ui;
		check(*this);
	}

	log::info
	{
		log, "'%s': Opened database @ `%s' with %zu columns at sequence number %lu.",
		this->name,
		path,
		columns.size(),
		d->GetLatestSequenceNumber()
	};
}
catch(const corruption &e)
{
	throw corruption
	{
		"Corruption for '%s' (%s). Try restarting with the -pitrecdb command line option",
		this->name,
		e.what()
	};
}
catch(const std::exception &e)
{
	throw error
	{
		"Failed to open db '%s': %s",
		this->name,
		e.what()
	};
}

ircd::db::database::~database()
noexcept try
{
	const ctx::uninterruptible::nothrow ui;

	log::info
	{
		log, "'%s': closing database @ `%s'...",
		name,
		path
	};

	rocksdb::CancelAllBackgroundWork(d.get(), true); // true = blocking
	log::debug
	{
		log, "'%s': background_errors: %lu; flushing...",
		name,
		property<uint64_t>(*this, rocksdb::DB::Properties::kBackgroundErrors)
	};

	flush(*this);
	this->checkpointer.reset(nullptr);
	this->columns.clear();
	log::debug
	{
		log, "'%s': closed columns; synchronizing...",
		name
	};

	sync(*this);
	log::debug
	{
		log, "'%s': synchronized with hardware.",
		name
	};

	const auto sequence
	{
		d->GetLatestSequenceNumber()
	};

	throw_on_error
	{
		d->Close()
	};

	log::info
	{
		log, "'%s': closed database @ `%s' at sequence number %lu.",
		name,
		path,
		sequence
	};
}
catch(const std::exception &e)
{
	log::error
	{
		log, "'%s': Error closing database(%p) :%s",
		name,
		this,
		e.what()
	};

	return;
}
catch(...)
{
	log::critical
	{
		log, "'%s': Unknown error closing database(%p)",
		name,
		this
	};

	return;
}

void
ircd::db::database::operator()(const delta &delta)
{
	operator()(sopts{}, delta);
}

void
ircd::db::database::operator()(const std::initializer_list<delta> &deltas)
{
	operator()(sopts{}, deltas);
}

void
ircd::db::database::operator()(const delta *const &begin,
                               const delta *const &end)
{
	operator()(sopts{}, begin, end);
}

void
ircd::db::database::operator()(const sopts &sopts,
                               const delta &delta)
{
	operator()(sopts, &delta, &delta + 1);
}

void
ircd::db::database::operator()(const sopts &sopts,
                               const std::initializer_list<delta> &deltas)
{
	operator()(sopts, std::begin(deltas), std::end(deltas));
}

void
ircd::db::database::operator()(const sopts &sopts,
                               const delta *const &begin,
                               const delta *const &end)
{
	rocksdb::WriteBatch batch;
	std::for_each(begin, end, [this, &batch]
	(const delta &delta)
	{
		const auto &op(std::get<op>(delta));
		const auto &col(std::get<1>(delta));
		const auto &key(std::get<2>(delta));
		const auto &val(std::get<3>(delta));
		db::column column(operator[](col));
		append(batch, column, db::column::delta
		{
			op,
			key,
			val
		});
	});

	commit(*this, batch, sopts);
}

ircd::db::database::column &
ircd::db::database::operator[](const string_view &name)
{
	const auto it{column_index.find(name)};
	if(unlikely(it == std::end(column_index)))
		throw schema_error
		{
			"'%s': column '%s' is not available or specified in schema",
			this->name,
			name
		};

	return operator[](it->second);
}

ircd::db::database::column &
ircd::db::database::operator[](const uint32_t &id)
try
{
	return *columns.at(id);
}
catch(const std::out_of_range &e)
{
	throw schema_error
	{
		"'%s': column id[%u] is not available or specified in schema",
		this->name,
		id
	};
}

const ircd::db::database::column &
ircd::db::database::operator[](const string_view &name)
const
{
	const auto it{column_index.find(name)};
	if(unlikely(it == std::end(column_index)))
		throw schema_error
		{
			"'%s': column '%s' is not available or specified in schema",
			this->name,
			name
		};

	return operator[](it->second);
}

const ircd::db::database::column &
ircd::db::database::operator[](const uint32_t &id)
const try
{
	return *columns.at(id);
}
catch(const std::out_of_range &e)
{
	throw schema_error
	{
		"'%s': column id[%u] is not available or specified in schema",
		this->name,
		id
	};
}

///////////////////////////////////////////////////////////////////////////////
//
// database::comparator
//

const char *
ircd::db::database::comparator::Name()
const noexcept
{
	assert(!user.name.empty());
	return user.name.data();
}

bool
ircd::db::database::comparator::Equal(const Slice &a,
                                      const Slice &b)
const noexcept
{
	return user.equal?
		user.equal(slice(a), slice(b)):
		Compare(a, b) == 0;
}

int
ircd::db::database::comparator::Compare(const Slice &a,
                                        const Slice &b)
const noexcept
{
	assert(bool(user.less));
	const auto sa{slice(a)};
	const auto sb{slice(b)};
	return user.less(sa, sb)?                -1:  // less[Y], equal[?], greater[?]
	       user.equal && user.equal(sa, sb)?  0:  // less[N], equal[Y], greater[?]
	       user.equal?                        1:  // less[N], equal[N], greater[Y]
	       user.less(sb, sa)?                 1:  // less[N], equal[?], greater[Y]
	                                          0;  // less[N], equal[Y], greater[N]
}

void
ircd::db::database::comparator::FindShortestSeparator(std::string *const key,
                                                      const Slice &limit)
const noexcept
{
	assert(key != nullptr);
	if(user.separator)
		user.separator(*key, slice(limit));
}

void
ircd::db::database::comparator::FindShortSuccessor(std::string *const key)
const noexcept
{
	assert(key != nullptr);
	if(user.successor)
		user.successor(*key);
}

///////////////////////////////////////////////////////////////////////////////
//
// database::prefix_transform
//

const char *
ircd::db::database::prefix_transform::Name()
const noexcept
{
	assert(!user.name.empty());
	return user.name.c_str();
}

rocksdb::Slice
ircd::db::database::prefix_transform::Transform(const Slice &key)
const noexcept
{
	assert(bool(user.get));
	return slice(user.get(slice(key)));
}

bool
ircd::db::database::prefix_transform::InRange(const Slice &key)
const noexcept
{
	return InDomain(key);
}

bool
ircd::db::database::prefix_transform::InDomain(const Slice &key)
const noexcept
{
	assert(bool(user.has));
	return user.has(slice(key));
}

///////////////////////////////////////////////////////////////////////////////
//
// database::column
//

void
ircd::db::drop(database::column &c)
{
	const ctx::uninterruptible ui;
	if(!c.handle)
		return;

	throw_on_error
	{
		c.d->d->DropColumnFamily(c.handle.get())
	};
}

uint32_t
ircd::db::id(const database::column &c)
{
	if(!c.handle)
		return -1;

	return c.handle->GetID();
}

const std::string &
ircd::db::name(const database::column &c)
{
	return c.name;
}

const ircd::db::database::descriptor &
ircd::db::describe(const database::column &c)
{
	return c.descriptor;
}

//
// database::column
//

ircd::db::database::column::column(database *const &d,
                                   const database::descriptor &descriptor)
:rocksdb::ColumnFamilyDescriptor
(
	descriptor.name, database::options{descriptor.options}
)
,d{d}
,key_type{descriptor.type.first}
,mapped_type{descriptor.type.second}
,descriptor{descriptor}
,cmp{d, this->descriptor.cmp}
,prefix{d, this->descriptor.prefix}
,handle
{
	nullptr, [this](rocksdb::ColumnFamilyHandle *const handle)
	{
		if(handle)
			this->d->d->DestroyColumnFamilyHandle(handle);
	}
}
{
	// If possible, deduce comparator based on type given in descriptor
	if(!this->descriptor.cmp.less)
	{
		if(key_type == typeid(string_view))
			this->cmp.user = cmp_string_view{};
		else if(key_type == typeid(int64_t))
			this->cmp.user = cmp_int64_t{};
		else if(key_type == typeid(uint64_t))
			this->cmp.user = cmp_uint64_t{};
		else
			throw error
			{
				"column '%s' key type[%s] requires user supplied comparator",
				this->name,
				key_type.name()
			};
	}

	// Set the key comparator
	this->options.comparator = &this->cmp;

	// Set the prefix extractor
	if(this->prefix.user.get && this->prefix.user.has)
		this->options.prefix_extractor = std::shared_ptr<const rocksdb::SliceTransform>
		{
			&this->prefix, [](const rocksdb::SliceTransform *) {}
		};

	//
	// Table options
	//

	// Setup the cache for assets.
	const auto &cache_size(this->descriptor.cache_size);
	if(cache_size)
		table_opts.block_cache = rocksdb::NewLRUCache(cache_size);

	// Setup the cache for compressed assets.
	const auto &cache_size_comp(this->descriptor.cache_size_comp);
	if(cache_size_comp)
		table_opts.block_cache_compressed = rocksdb::NewLRUCache(cache_size_comp);

	// Setup the bloom filter.
	const auto &bloom_bits(this->descriptor.bloom_bits);
	if(bloom_bits)
		table_opts.filter_policy.reset(rocksdb::NewBloomFilterPolicy(bloom_bits, false));

	// Tickers::READ_AMP_TOTAL_READ_BYTES / Tickers::READ_AMP_ESTIMATE_USEFUL_BYTES
	//table_opts.read_amp_bytes_per_bit = 8;

	this->options.table_factory.reset(rocksdb::NewBlockBasedTableFactory(table_opts));

	//
	// Misc options
	//

	// Set the compaction style; we don't override this in the descriptor yet.
	this->options.compaction_style = rocksdb::kCompactionStyleLevel;

	// Set the compaction priority; this should probably be in the descriptor
	// but this is currently selected for the general matrix workload.
	this->options.compaction_pri = rocksdb::CompactionPri::kOldestSmallestSeqFirst;

	// Set filter reductions for this column. This means we expect a key to exist.
	this->options.optimize_filters_for_hits = this->descriptor.expect_queries_hit;

	// Compression
	//TODO: descriptor / conf / detect etc...
	//this->options.compression = rocksdb::kSnappyCompression;

	//TODO: descriptor / conf
	this->options.num_levels = 8;
	this->options.target_file_size_base = 64_MiB;
	this->options.target_file_size_multiplier = 4;        // size at level
	this->options.level0_file_num_compaction_trigger = 2;

	log::debug
	{
		log, "schema '%s' column [%s => %s] cmp[%s] pfx[%s] lru:%zu:%zu bloom:%zu %s",
		db::name(*d),
		demangle(key_type.name()),
		demangle(mapped_type.name()),
		this->cmp.Name(),
		this->options.prefix_extractor? this->prefix.Name() : "none",
		cache_size,
		cache_size_comp,
		bloom_bits,
		this->descriptor.name
	};
}

ircd::db::database::column::~column()
noexcept
{
}

ircd::db::database::column::operator
database &()
{
	return *d;
}

ircd::db::database::column::operator
rocksdb::ColumnFamilyHandle *()
{
	return handle.get();
}

ircd::db::database::column::operator
const database &()
const
{
	return *d;
}

ircd::db::database::column::operator
const rocksdb::ColumnFamilyHandle *()
const
{
	return handle.get();
}

///////////////////////////////////////////////////////////////////////////////
//
// database::snapshot
//

uint64_t
ircd::db::sequence(const database::snapshot &s)
{
	const rocksdb::Snapshot *const rs(s);
	return sequence(rs);
}

uint64_t
ircd::db::sequence(const rocksdb::Snapshot *const &rs)
{
	return likely(rs)? rs->GetSequenceNumber() : 0ULL;
}

ircd::db::database::snapshot::snapshot(database &d)
:s
{
	d.d->GetSnapshot(),
	[dp(weak_from(d))](const rocksdb::Snapshot *const s)
	{
		if(!s)
			return;

		const auto d(dp.lock());
		d->d->ReleaseSnapshot(s);
	}
}
{
}

ircd::db::database::snapshot::~snapshot()
noexcept
{
}

///////////////////////////////////////////////////////////////////////////////
//
// database::logs
//

ircd::db::database::logs::logs(database *const &d)
:rocksdb::Logger{}
,d{d}
{
}

ircd::db::database::logs::~logs()
noexcept
{
}

rocksdb::Status
ircd::db::database::logs::Close()
noexcept
{
	return rocksdb::Status::NotSupported();
}

static
ircd::log::facility
translate(const rocksdb::InfoLogLevel &level)
{
	switch(level)
	{
		// Treat all infomational messages from rocksdb as debug here for now.
		// We can clean them up and make better reports for our users eventually.
		default:
		case rocksdb::InfoLogLevel::DEBUG_LEVEL:     return ircd::log::facility::DEBUG;
		case rocksdb::InfoLogLevel::INFO_LEVEL:      return ircd::log::facility::DEBUG;

		case rocksdb::InfoLogLevel::WARN_LEVEL:      return ircd::log::facility::WARNING;
		case rocksdb::InfoLogLevel::ERROR_LEVEL:     return ircd::log::facility::ERROR;
		case rocksdb::InfoLogLevel::FATAL_LEVEL:     return ircd::log::facility::CRITICAL;
		case rocksdb::InfoLogLevel::HEADER_LEVEL:    return ircd::log::facility::NOTICE;
	}
}

void
ircd::db::database::logs::Logv(const char *const fmt,
                               va_list ap)
noexcept
{
	Logv(rocksdb::InfoLogLevel::DEBUG_LEVEL, fmt, ap);
}

void
ircd::db::database::logs::LogHeader(const char *const fmt,
                                    va_list ap)
noexcept
{
	Logv(rocksdb::InfoLogLevel::DEBUG_LEVEL, fmt, ap);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsuggest-attribute=format"
void
ircd::db::database::logs::Logv(const rocksdb::InfoLogLevel level,
                               const char *const fmt,
                               va_list ap)
noexcept
{
	if(level < GetInfoLogLevel())
		return;

	thread_local char buf[1024]; const auto len
	{
		vsnprintf(buf, sizeof(buf), fmt, ap)
	};

	const auto str
	{
		// RocksDB adds annoying leading whitespace to attempt to right-justify things and idc
		lstrip(string_view{buf, size_t(len)}, ' ')
	};

	// Skip the options for now
	if(startswith(str, "Options"))
		return;

	rog(translate(level), "'%s': %s", d->name, str);
}
#pragma GCC diagnostic pop

///////////////////////////////////////////////////////////////////////////////
//
// database::mergeop
//

ircd::db::database::mergeop::mergeop(database *const &d,
                                     merge_closure merger)
:d{d}
,merger
{
	merger?
		std::move(merger):
		ircd::db::merge_operator
}
{
}

ircd::db::database::mergeop::~mergeop()
noexcept
{
}

const char *
ircd::db::database::mergeop::Name()
const noexcept
{
	return "<unnamed>";
}

bool
ircd::db::database::mergeop::Merge(const rocksdb::Slice &_key,
                                   const rocksdb::Slice *const _exist,
                                   const rocksdb::Slice &_update,
                                   std::string *const newval,
                                   rocksdb::Logger *const)
const noexcept try
{
	const string_view key
	{
		_key.data(), _key.size()
	};

	const string_view exist
	{
		_exist? string_view { _exist->data(), _exist->size() } : string_view{}
	};

	const string_view update
	{
		_update.data(), _update.size()
	};

	if(exist.empty())
	{
		*newval = std::string(update);
		return true;
	}

	//XXX caching opportunity?
	*newval = merger(key, {exist, update});   // call the user
	return true;
}
catch(const std::bad_function_call &e)
{
	log.critical("merge: missing merge operator (%s)", e);
	return false;
}
catch(const std::exception &e)
{
	log.error("merge: %s", e);
	return false;
}

///////////////////////////////////////////////////////////////////////////////
//
// db/stats.h
//

std::string
ircd::db::string(const rocksdb::IOStatsContext &ic,
                 const bool &all)
{
	const bool exclude_zeros(!all);
	return ic.ToString(exclude_zeros);
}

const rocksdb::IOStatsContext &
ircd::db::iostats_current()
{
	const auto *const &ret
	{
		rocksdb::get_iostats_context()
	};

	if(unlikely(!ret))
		throw error
		{
			"IO counters are not available on this thread."
		};

	return *ret;
}

std::string
ircd::db::string(const rocksdb::PerfContext &pc,
                 const bool &all)
{
	const bool exclude_zeros(!all);
	return pc.ToString(exclude_zeros);
}

const rocksdb::PerfContext &
ircd::db::perf_current()
{
	const auto *const &ret
	{
		rocksdb::get_perf_context()
	};

	if(unlikely(!ret))
		throw error
		{
			"Performance counters are not available on this thread."
		};

	return *ret;
}

void
ircd::db::perf_level(const uint &level)
{
	if(level >= rocksdb::PerfLevel::kOutOfBounds)
		throw error
		{
			"Perf level of '%u' is invalid; maximum is '%u'",
			level,
			uint(rocksdb::PerfLevel::kOutOfBounds)
		};

	rocksdb::SetPerfLevel(rocksdb::PerfLevel(level));
}

uint
ircd::db::perf_level()
{
	return rocksdb::GetPerfLevel();
}

uint32_t
ircd::db::ticker_id(const string_view &key)
{
	for(const auto &pair : rocksdb::TickersNameMap)
		if(key == pair.second)
			return pair.first;

	throw std::out_of_range
	{
		"No ticker with that key"
	};
}

ircd::string_view
ircd::db::ticker_id(const uint32_t &id)
{
	for(const auto &pair : rocksdb::TickersNameMap)
		if(id == pair.first)
			return pair.second;

	return {};
}

decltype(ircd::db::ticker_max)
ircd::db::ticker_max
{
	rocksdb::TICKER_ENUM_MAX
};

uint32_t
ircd::db::histogram_id(const string_view &key)
{
	for(const auto &pair : rocksdb::HistogramsNameMap)
		if(key == pair.second)
			return pair.first;

	throw std::out_of_range
	{
		"No histogram with that key"
	};
}

ircd::string_view
ircd::db::histogram_id(const uint32_t &id)
{
	for(const auto &pair : rocksdb::HistogramsNameMap)
		if(id == pair.first)
			return pair.second;

	return {};
}

decltype(ircd::db::histogram_max)
ircd::db::histogram_max
{
	rocksdb::HISTOGRAM_ENUM_MAX
};

///////////////////////////////////////////////////////////////////////////////
//
// database::stats (db/database/stats.h) internal
//

uint64_t
ircd::db::database::stats::getAndResetTickerCount(const uint32_t type)
noexcept
{
	const auto ret(getTickerCount(type));
	setTickerCount(type, 0);
	return ret;
}

bool
ircd::db::database::stats::HistEnabledForType(const uint32_t type)
const noexcept
{
	return type < histogram.size();
}

void
ircd::db::database::stats::measureTime(const uint32_t type,
                                       const uint64_t time)
noexcept
{
}

void
ircd::db::database::stats::histogramData(const uint32_t type,
                                         rocksdb::HistogramData *const data)
const noexcept
{
	assert(data);

	const auto &median(data->median);
	const auto &percentile95(data->percentile95);
	const auto &percentile88(data->percentile99);
	const auto &average(data->average);
	const auto &standard_deviation(data->standard_deviation);
}

void
ircd::db::database::stats::recordTick(const uint32_t type,
                                      const uint64_t count)
noexcept
{
	ticker.at(type) += count;
}

void
ircd::db::database::stats::setTickerCount(const uint32_t type,
                                          const uint64_t count)
noexcept
{
	ticker.at(type) = count;
}

uint64_t
ircd::db::database::stats::getTickerCount(const uint32_t type)
const noexcept
{
	return ticker.at(type);
}

///////////////////////////////////////////////////////////////////////////////
//
// database::events
//

void
ircd::db::database::events::OnFlushCompleted(rocksdb::DB *const db,
                                             const rocksdb::FlushJobInfo &info)
noexcept
{
	rog.debug("'%s' @%p: flushed: column[%s] path[%s] tid[%lu] job[%d] writes[slow:%d stop:%d]",
	          d->name,
	          db,
	          info.cf_name,
	          info.file_path,
	          info.thread_id,
	          info.job_id,
	          info.triggered_writes_slowdown,
	          info.triggered_writes_stop);
}

void
ircd::db::database::events::OnCompactionCompleted(rocksdb::DB *const db,
                                                  const rocksdb::CompactionJobInfo &info)
noexcept
{
	rog.debug("'%s' @%p: compacted: column[%s] status[%d] tid[%lu] job[%d]",
	          d->name,
	          db,
	          info.cf_name,
	          int(info.status.code()),
	          info.thread_id,
	          info.job_id);
}

void
ircd::db::database::events::OnTableFileDeleted(const rocksdb::TableFileDeletionInfo &info)
noexcept
{
	rog.debug("'%s': table file deleted: db[%s] path[%s] status[%d] job[%d]",
	          d->name,
	          info.db_name,
	          info.file_path,
	          int(info.status.code()),
	          info.job_id);
}

void
ircd::db::database::events::OnTableFileCreated(const rocksdb::TableFileCreationInfo &info)
noexcept
{
	rog.debug("'%s': table file created: db[%s] path[%s] status[%d] job[%d]",
	          d->name,
	          info.db_name,
	          info.file_path,
	          int(info.status.code()),
	          info.job_id);
}

void
ircd::db::database::events::OnTableFileCreationStarted(const rocksdb::TableFileCreationBriefInfo &info)
noexcept
{
	rog.debug("'%s': table file creating: db[%s] column[%s] path[%s] job[%d]",
	          d->name,
	          info.db_name,
	          info.cf_name,
	          info.file_path,
	          info.job_id);
}

void
ircd::db::database::events::OnMemTableSealed(const rocksdb::MemTableInfo &info)
noexcept
{
	rog.debug("'%s': memory table sealed: column[%s] entries[%lu] deletes[%lu]",
	          d->name,
	          info.cf_name,
	          info.num_entries,
	          info.num_deletes);
}

void
ircd::db::database::events::OnColumnFamilyHandleDeletionStarted(rocksdb::ColumnFamilyHandle *const h)
noexcept
{
	rog.debug("'%s': column[%s] handle closing @ %p",
	          d->name,
	          h->GetName(),
	          h);
}

///////////////////////////////////////////////////////////////////////////////
//
// database::env
//

//
// env::state
//

ircd::db::database::env::state::state(database *const &d)
:d{*d}
{
}

ircd::db::database::env::state::~state()
noexcept
{
	for(auto &p : pool) try
	{
		p.terminate();
		p.join();
	}
	catch(...)
	{
		continue;
	}
}

//
// env::env
//

ircd::db::database::env::env(database *const &d)
:d{*d},
st{std::make_unique<state>(d)}
{
}

ircd::db::database::env::~env()
noexcept
{
}

rocksdb::Status
ircd::db::database::env::NewSequentialFile(const std::string &name,
                                           std::unique_ptr<SequentialFile> *const r,
                                           const EnvOptions &options)
noexcept try
{
	const ctx::uninterruptible::nothrow ui;

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': new sequential file '%s' options:%p",
		d.name,
		name,
		&options
	};
	#endif

	*r = std::make_unique<sequential_file>(&d, name, options);
	return Status::OK();
}
catch(const fs::error &e)
{
	return error_to_status{e};
}
catch(const std::exception &e)
{
	return error_to_status{e};
}

rocksdb::Status
ircd::db::database::env::NewRandomAccessFile(const std::string &name,
                                             std::unique_ptr<RandomAccessFile> *const r,
                                             const EnvOptions &options)
noexcept try
{
	ctx::uninterruptible::nothrow ui;

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': new random access file '%s' options:%p",
		d.name,
		name,
		&options
	};
	#endif

	*r = std::make_unique<random_access_file>(&d, name, options);
	return Status::OK();
}
catch(const fs::error &e)
{
	return error_to_status{e};
}
catch(const std::exception &e)
{
	return error_to_status{e};
}

rocksdb::Status
ircd::db::database::env::NewWritableFile(const std::string &name,
                                         std::unique_ptr<WritableFile> *const r,
                                         const EnvOptions &options)
noexcept try
{
	const ctx::uninterruptible::nothrow ui;

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': new writable file '%s' options:%p",
		d.name,
		name,
		&options
	};
	#endif

	*r = std::make_unique<writable_file>(&d, name, options, true);
	return Status::OK();
}
catch(const fs::error &e)
{
	return error_to_status{e};
}
catch(const std::exception &e)
{
	return error_to_status{e};
}

rocksdb::Status
ircd::db::database::env::ReopenWritableFile(const std::string &name,
                                            std::unique_ptr<WritableFile> *const r,
                                            const EnvOptions &options)
noexcept try
{
	const ctx::uninterruptible::nothrow ui;

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': reopen writable file '%s' options:%p",
		d.name,
		name,
		&options
	};
	#endif

	*r = std::make_unique<writable_file>(&d, name, options, false);
	return Status::OK();
}
catch(const fs::error &e)
{
	return error_to_status{e};
}
catch(const std::exception &e)
{
	return error_to_status{e};
}

rocksdb::Status
ircd::db::database::env::ReuseWritableFile(const std::string &name,
                                           const std::string &old_name,
                                           std::unique_ptr<WritableFile> *const r,
                                           const EnvOptions &options)
noexcept try
{
	const ctx::uninterruptible::nothrow ui;

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': reuse writable file '%s' old '%s' options:%p",
		d.name,
		name,
		old_name,
		&options
	};
	#endif

	assert(0);
	return Status::OK();
	//return defaults.ReuseWritableFile(name, old_name, r, options);
}
catch(const fs::error &e)
{
	return error_to_status{e};
}
catch(const std::exception &e)
{
	return error_to_status{e};
}

rocksdb::Status
ircd::db::database::env::NewRandomRWFile(const std::string &name,
                                         std::unique_ptr<RandomRWFile> *const result,
                                         const EnvOptions &options)
noexcept try
{
	const ctx::uninterruptible::nothrow ui;

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': new random read/write file '%s' options:%p",
		d.name,
		name,
		&options
	};
	#endif

	*result = std::make_unique<random_rw_file>(&d, name, options);
	return Status::OK();
}
catch(const fs::error &e)
{
	return error_to_status{e};
}
catch(const std::exception &e)
{
	return error_to_status{e};
}

rocksdb::Status
ircd::db::database::env::NewDirectory(const std::string &name,
                                      std::unique_ptr<Directory> *const result)
noexcept try
{
	const ctx::uninterruptible::nothrow ui;

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': new directory '%s'",
		d.name,
		name
	};
	#endif

	std::unique_ptr<Directory> defaults;
	const auto ret
	{
		this->defaults.NewDirectory(name, &defaults)
	};

	*result = std::make_unique<directory>(&d, name, std::move(defaults));
	return ret;
}
catch(const fs::error &e)
{
	return error_to_status{e};
}
catch(const std::exception &e)
{
	return error_to_status{e};
}

rocksdb::Status
ircd::db::database::env::FileExists(const std::string &f)
noexcept try
{
	const ctx::uninterruptible::nothrow ui;

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': file exists '%s'",
		d.name,
		f
	};
	#endif

	return defaults.FileExists(f);
}
catch(const fs::error &e)
{
	return error_to_status{e};
}
catch(const std::exception &e)
{
	return error_to_status{e};
}

rocksdb::Status
ircd::db::database::env::GetChildren(const std::string &dir,
                                     std::vector<std::string> *const r)
noexcept try
{
	const ctx::uninterruptible::nothrow ui;

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': get children of directory '%s'",
		d.name,
		dir
	};
	#endif

	return defaults.GetChildren(dir, r);
}
catch(const fs::error &e)
{
	return error_to_status{e};
}
catch(const std::exception &e)
{
	return error_to_status{e};
}

rocksdb::Status
ircd::db::database::env::GetChildrenFileAttributes(const std::string &dir,
                                                   std::vector<FileAttributes> *const result)
noexcept try
{
	const ctx::uninterruptible::nothrow ui;

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': get children file attributes of directory '%s'",
		d.name,
		dir
	};
	#endif

	return defaults.GetChildrenFileAttributes(dir, result);
}
catch(const fs::error &e)
{
	return error_to_status{e};
}
catch(const std::exception &e)
{
	return error_to_status{e};
}

rocksdb::Status
ircd::db::database::env::DeleteFile(const std::string &name)
noexcept try
{
	const ctx::uninterruptible::nothrow ui;

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': delete file '%s'",
		d.name,
		name
	};
	#endif

	return defaults.DeleteFile(name);
}
catch(const fs::error &e)
{
	return error_to_status{e};
}
catch(const std::exception &e)
{
	return error_to_status{e};
}

rocksdb::Status
ircd::db::database::env::CreateDir(const std::string &name)
noexcept try
{
	const ctx::uninterruptible::nothrow ui;

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': create directory '%s'",
		d.name,
		name
	};
	#endif

	return defaults.CreateDir(name);
}
catch(const fs::error &e)
{
	return error_to_status{e};
}
catch(const std::exception &e)
{
	return error_to_status{e};
}

rocksdb::Status
ircd::db::database::env::CreateDirIfMissing(const std::string &name)
noexcept try
{
	const ctx::uninterruptible::nothrow ui;

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': create directory if missing '%s'",
		d.name,
		name
	};
	#endif

	return defaults.CreateDirIfMissing(name);
}
catch(const fs::error &e)
{
	return error_to_status{e};
}
catch(const std::exception &e)
{
	return error_to_status{e};
}

rocksdb::Status
ircd::db::database::env::DeleteDir(const std::string &name)
noexcept try
{
	const ctx::uninterruptible::nothrow ui;

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': delete directory '%s'",
		d.name,
		name
	};
	#endif

	return defaults.DeleteDir(name);
}
catch(const fs::error &e)
{
	return error_to_status{e};
}
catch(const std::exception &e)
{
	return error_to_status{e};
}

rocksdb::Status
ircd::db::database::env::GetFileSize(const std::string &name,
                                     uint64_t *const s)
noexcept try
{
	const ctx::uninterruptible::nothrow ui;

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': get file size '%s'",
		d.name,
		name
	};
	#endif

	assert(s);
	*s = fs::size(name);
	return Status::OK();
}
catch(const fs::error &e)
{
	return error_to_status{e};
}
catch(const std::exception &e)
{
	return error_to_status{e};
}

rocksdb::Status
ircd::db::database::env::GetFileModificationTime(const std::string &name,
                                                 uint64_t *const file_mtime)
noexcept try
{
	const ctx::uninterruptible::nothrow ui;

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': get file mtime '%s'",
		d.name,
		name
	};
	#endif

	return defaults.GetFileModificationTime(name, file_mtime);
}
catch(const fs::error &e)
{
	return error_to_status{e};
}
catch(const std::exception &e)
{
	return error_to_status{e};
}

rocksdb::Status
ircd::db::database::env::RenameFile(const std::string &s,
                                    const std::string &t)
noexcept try
{
	const ctx::uninterruptible::nothrow ui;

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': rename file '%s' to '%s'",
		d.name,
		s,
		t
	};
	#endif

	return defaults.RenameFile(s, t);
}
catch(const fs::error &e)
{
	return error_to_status{e};
}
catch(const std::exception &e)
{
	return error_to_status{e};
}

rocksdb::Status
ircd::db::database::env::LinkFile(const std::string &s,
                                  const std::string &t)
noexcept try
{
	const ctx::uninterruptible::nothrow ui;

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': link file '%s' to '%s'",
		d.name,
		s,
		t
	};
	#endif

	return defaults.LinkFile(s, t);
}
catch(const fs::error &e)
{
	return error_to_status{e};
}
catch(const std::exception &e)
{
	return error_to_status{e};
}

rocksdb::Status
ircd::db::database::env::LockFile(const std::string &name,
                                  FileLock** l)
noexcept try
{
	const ctx::uninterruptible::nothrow ui;

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': lock file '%s'",
		d.name,
		name
	};
	#endif

	return defaults.LockFile(name, l);
}
catch(const fs::error &e)
{
	return error_to_status{e};
}
catch(const std::exception &e)
{
	return error_to_status{e};
}

rocksdb::Status
ircd::db::database::env::UnlockFile(FileLock *const l)
noexcept try
{
	const ctx::uninterruptible::nothrow ui;

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': unlock file lock:%p",
		d.name,
		l
	};
	#endif

	return defaults.UnlockFile(l);
}
catch(const fs::error &e)
{
	return error_to_status{e};
}
catch(const std::exception &e)
{
	return error_to_status{e};
}

rocksdb::Status
ircd::db::database::env::GetTestDirectory(std::string *const path)
noexcept try
{
	const ctx::uninterruptible::nothrow ui;

	return defaults.GetTestDirectory(path);
}
catch(const fs::error &e)
{
	return error_to_status{e};
}
catch(const std::exception &e)
{
	return error_to_status{e};
}

rocksdb::Status
ircd::db::database::env::GetAbsolutePath(const std::string &db_path,
                                         std::string *const output_path)
noexcept try
{
	const ctx::uninterruptible::nothrow ui;

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': get absolute path from '%s' ret:%p",
		d.name,
		db_path,
		output_path
	};
	#endif

	return defaults.GetAbsolutePath(db_path, output_path);
}
catch(const fs::error &e)
{
	return error_to_status{e};
}
catch(const std::exception &e)
{
	return error_to_status{e};
}

rocksdb::Status
ircd::db::database::env::NewLogger(const std::string &name,
                                   std::shared_ptr<Logger> *const result)
noexcept try
{
	const ctx::uninterruptible::nothrow ui;

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': new logger '%s' result:%p",
		d.name,
		name,
		(const void *)result
	};
	#endif

	return defaults.NewLogger(name, result);
}
catch(const std::exception &e)
{
	return error_to_status{e};
}

rocksdb::Status
ircd::db::database::env::GetHostName(char *const name,
                                     uint64_t len)
noexcept try
{
	const ctx::uninterruptible::nothrow ui;

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': get host name name:%p len:%lu",
		d.name,
		name,
		len
	};
	#endif

	return defaults.GetHostName(name, len);
}
catch(const std::exception &e)
{
	return error_to_status{e};
}

uint64_t
ircd::db::database::env::NowMicros()
noexcept try
{
	return defaults.NowMicros();
}
catch(const std::exception &e)
{
	throw assertive
	{
		"'%s': now micros :%s",
		d.name,
		e.what()
	};
}

rocksdb::Status
ircd::db::database::env::GetCurrentTime(int64_t *const unix_time)
noexcept try
{
	return defaults.GetCurrentTime(unix_time);
}
catch(const std::exception &e)
{
	return error_to_status{e};
}

std::string
ircd::db::database::env::TimeToString(uint64_t time)
noexcept try
{
	return defaults.TimeToString(time);
}
catch(const std::exception &e)
{
	throw assertive
	{
		"'%s': time to string :%s",
		d.name,
		e.what()
	};
}

void
ircd::db::database::env::SleepForMicroseconds(int micros)
noexcept try
{
	const ctx::uninterruptible::nothrow ui;

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		"'%s': sleep for %d microseconds",
		d.name,
		micros
	};
	#endif

	ctx::sleep(microseconds(micros));
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "'%s': sleep micros:%d :%s",
		d.name,
		micros,
		e.what()
	};
}

void
ircd::db::database::env::Schedule(void (*f)(void* arg),
                                  void *const a,
                                  Priority prio,
                                  void *const tag,
                                  void (*u)(void* arg))
noexcept try
{
	const ctx::uninterruptible::nothrow ui;

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': schedule func:%p a:%p tag:%p u:%p prio:%s",
		d.name,
		f,
		a,
		tag,
		u,
		reflect(prio)
	};
	#endif

	assert(st);
	auto &pool
	{
		st->pool.at(prio)
	};

	auto &tasks
	{
		st->tasks.at(prio)
	};

	tasks.emplace_back(state::task
	{
		f, u, a
	});

	pool([this, &tasks]
	{
		ctx::uninterruptible::nothrow ui;

		assert(this->st);
		if(tasks.empty())
			return;

		const auto task
		{
			std::move(tasks.front())
		};

		tasks.pop_front();

		#ifdef RB_DEBUG_DB_ENV
		log::debug
		{
			log, "'%s': func:%p arg:%p",
			this->d.name,
			task.func,
			task.arg,
		};
		#endif

		// Execute the task
		task.func(task.arg);
	});
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "'%s': schedule func:%p a:%p tag:%p u:%p prio:%s",
		d.name,
		f,
		a,
		tag,
		u,
		reflect(prio)
	};
}

int
ircd::db::database::env::UnSchedule(void *const tag,
                                    const Priority prio)
noexcept try
{
	ctx::uninterruptible::nothrow ui;

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': unschedule tag:%p prio:%s",
		d.name,
		tag,
		reflect(prio)
	};
	#endif

	assert(st);
	auto &tasks
	{
		st->tasks.at(prio)
	};

	size_t i(0);
	for(auto it(begin(tasks)); it != end(tasks); it = tasks.erase(it), ++i)
		it->cancel(it->arg);

	return i;
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "'%s': unschedule tag:%p prio:%s :%s",
		d.name,
		tag,
		reflect(prio),
		e.what()
	};

	return 0;
}

void
ircd::db::database::env::StartThread(void (*f)(void*),
                                     void *const a)
noexcept
{
	const ctx::uninterruptible::nothrow ui;

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': start thread func:%p a:%p",
		d.name,
		f,
		a
	};
	#endif

	throw assertive
	{
		"Independent (non-pool) context spawning not yet implemented"
	};
}

void
ircd::db::database::env::WaitForJoin()
noexcept try
{
	const ctx::uninterruptible::nothrow ui;

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': wait for all ctx to join",
		d.name
	};
	#endif

	assert(st);
	for(auto &pool : st->pool)
		pool.join();
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "'%s': wait for join :%s",
		d.name,
		e.what()
	};
}

unsigned int
ircd::db::database::env::GetThreadPoolQueueLen(Priority prio)
const noexcept try
{
	const ctx::uninterruptible::nothrow ui;

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': get thread pool queue len prio:%s",
		d.name,
		reflect(prio)
	};
	#endif

	assert(st);
	const auto &pool
	{
		st->pool.at(prio)
	};

	return pool.queued();
}
catch(const std::exception &e)
{
	throw assertive
	{
		"'%s': set background threads :%s",
		d.name,
		e.what()
	};
}

void
ircd::db::database::env::SetBackgroundThreads(int num,
                                              Priority prio)
noexcept try
{
	ctx::uninterruptible::nothrow ui;

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': set background threads num:%d prio:%s",
		d.name,
		num,
		reflect(prio)
	};
	#endif

	assert(st);
	auto &pool
	{
		st->pool.at(prio)
	};

	const auto &size
	{
		ssize_t(pool.size())
	};

	if(size > num)
		pool.del(size - num);
	else if(size < num)
		pool.add(num - size);
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "'%s': set background threads :%s",
		d.name,
		e.what()
	};
}

void
ircd::db::database::env::IncBackgroundThreadsIfNeeded(int num,
                                                      Priority prio)
noexcept try
{
	ctx::uninterruptible::nothrow ui;

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': increase background threads num:%d prio:%s",
		d.name,
		num,
		reflect(prio)
	};
	#endif

	assert(st);
	auto &pool
	{
		st->pool.at(prio)
	};

	pool.add(num);
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "'%s': inc background threads num:%d prio:%s :%s",
		d.name,
		num,
		reflect(prio),
		e.what()
	};
}

void
ircd::db::database::env::LowerThreadPoolIOPriority(Priority pool)
noexcept try
{
	const ctx::uninterruptible::nothrow ui;

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': lower thread pool priority prio:%s",
		d.name,
		reflect(pool)
	};
	#endif

	defaults.LowerThreadPoolIOPriority(pool);
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "'%s': lower thread pool IO priority pool:%s :%s",
		d.name,
		reflect(pool),
		e.what()
	};
}

rocksdb::Status
ircd::db::database::env::GetThreadList(std::vector<ThreadStatus> *const list)
noexcept try
{
	const ctx::uninterruptible::nothrow ui;

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': get thread list %p (%zu)",
		d.name,
		list,
		list? list->size() : 0UL
	};
	#endif

	assert(0);
	return defaults.GetThreadList(list);
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "'%s': get thread list:%p :%s",
		d.name,
		list,
		e.what()
	};

	return error_to_status{e};
}

rocksdb::ThreadStatusUpdater *
ircd::db::database::env::GetThreadStatusUpdater()
const noexcept try
{
	const ctx::uninterruptible::nothrow ui;

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': get thread status updater",
		d.name,
	};
	#endif

	return defaults.GetThreadStatusUpdater();
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "'%s': get thread status updater :%s",
		d.name,
		e.what()
	};

	return nullptr;
}


uint64_t
ircd::db::database::env::GetThreadID()
const noexcept try
{
	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': get thread ID",
		d.name,
	};
	#endif

	return ctx::this_ctx::id();
}
catch(const std::exception &e)
{
	throw assertive
	{
		"'%s': get thread id :%s",
		d.name,
		e.what()
	};
}

int
ircd::db::database::env::GetBackgroundThreads(Priority prio)
noexcept try
{
	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': get background threads prio:%s",
		d.name,
		reflect(prio)
	};
	#endif

	assert(st);
	const auto &pool
	{
		st->pool.at(prio)
	};

	return pool.size();
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "'%s': get background threads prio:%s :%s",
		d.name,
		reflect(prio),
		e.what()
	};

	return 0;
}

//
// writable_file
//

ircd::db::database::env::writable_file::writable_file(database *const &d,
                                                      const std::string &name,
                                                      const EnvOptions &env_opts,
                                                      const bool &trunc)
try
:d
{
	*d
}
,env_opts
{
	env_opts
}
,opts{[this, &trunc]
{
	fs::fd::opts ret
	{
		std::ios_base::out |
		(trunc? std::ios::trunc : 0)
	};

	ret.direct = this->env_opts.use_direct_writes;
	ret.cloexec = this->env_opts.set_fd_cloexec;
	return ret;
}()}
,fd
{
	name, this->opts
}
{
	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': opened wfile:%p fd:%d '%s'",
		d->name,
		this,
		int(fd),
		name
	};
	#endif
}
catch(const std::exception &e)
{
	log::error
	{
		log, "'%s': opening wfile:%p `%s' :%s",
		d->name,
		this,
		name,
		e.what()
	};
}

ircd::db::database::env::writable_file::~writable_file()
noexcept
{
	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': closed wfile:%p fd:%d",
		d.name,
		this,
		int(fd)
	};
	#endif
}

rocksdb::Status
ircd::db::database::env::writable_file::Close()
noexcept try
{
	const ctx::uninterruptible::nothrow ui;
	const std::lock_guard<decltype(mutex)> lock{mutex};

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': wfile:%p fd:%d close",
		d.name,
		this,
		int(fd)
	};
	#endif

	this->fd = fs::fd{};
	return Status::OK();
}
catch(const fs::error &e)
{
	log::error
	{
		log, "'%s': wfile:%p close :%s",
		d.name,
		this,
		e.what()
	};

	return error_to_status{e};
}
catch(const std::exception &e)
{
	log::error
	{
		log, "'%s': wfile:%p close :%s",
		d.name,
		this,
		e.what()
	};

	return error_to_status{e};
}

rocksdb::Status
ircd::db::database::env::writable_file::Flush()
noexcept try
{
	const ctx::uninterruptible::nothrow ui;
	const std::lock_guard<decltype(mutex)> lock{mutex};

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': wfile:%p fd:%d flush",
		d.name,
		this,
		int(fd),
	};
	#endif

	fs::fdsync(fd);
	return Status::OK();
}
catch(const fs::error &e)
{
	log::error
	{
		log, "'%s': wfile:%p fd:%d flush :%s",
		d.name,
		this,
		int(fd),
		e.what()
	};

	return error_to_status{e};
}
catch(const std::exception &e)
{
	log::error
	{
		log, "'%s': wfile:%p fd:%d flush :%s",
		d.name,
		this,
		int(fd),
		e.what()
	};

	return error_to_status{e};
}

rocksdb::Status
ircd::db::database::env::writable_file::Sync()
noexcept try
{
	const ctx::uninterruptible::nothrow ui;
	const std::lock_guard<decltype(mutex)> lock{mutex};

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': wfile:%p sync",
		d.name,
		this
	};
	#endif

	fs::fdsync(fd);
	return Status::OK();
}
catch(const fs::error &e)
{
	log::error
	{
		log, "'%s': wfile:%p sync :%s",
		d.name,
		this,
		e.what()
	};

	return error_to_status{e};
}
catch(const std::exception &e)
{
	log::error
	{
		log, "'%s': wfile:%p sync :%s",
		d.name,
		this,
		e.what()
	};

	return error_to_status{e};
}

rocksdb::Status
ircd::db::database::env::writable_file::Fsync()
noexcept try
{
	const ctx::uninterruptible::nothrow ui;
	const std::lock_guard<decltype(mutex)> lock{mutex};

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': wfile:%p fsync",
		d.name,
		this
	};
	#endif

	fs::fsync(fd);
	return Status::OK();
}
catch(const fs::error &e)
{
	log::error
	{
		log, "'%s': wfile:%p fsync :%s",
		d.name,
		this,
		e.what()
	};

	return error_to_status{e};
}
catch(const std::exception &e)
{
	log::error
	{
		log, "'%s': wfile:%p fsync :%s",
		d.name,
		this,
		e.what()
	};

	return error_to_status{e};
}

rocksdb::Status
ircd::db::database::env::writable_file::RangeSync(uint64_t offset,
                                                  uint64_t length)
noexcept try
{
	const ctx::uninterruptible::nothrow ui;
	const std::lock_guard<decltype(mutex)> lock{mutex};

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		"'%s': wfile:%p fd:%d range sync offset:%lu length:%lu",
		d.name,
		this,
		int(fd),
		offset,
		length
	};
	#endif

	assert(0);
	return Status::NotSupported();
}
catch(const fs::error &e)
{
	log::error
	{
		log, "'%s': wfile:%p fd:%d range sync offset:%zu length:%zu :%s",
		d.name,
		this,
		int(fd),
		offset,
		length,
		e.what()
	};

	return error_to_status{e};
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "'%s': wfile:%p fd:%d range sync offset:%zu length:%zu :%s",
		d.name,
		this,
		int(fd),
		offset,
		length,
		e.what()
	};

	return error_to_status{e};
}

rocksdb::Status
ircd::db::database::env::writable_file::Append(const Slice &s)
noexcept try
{
	const ctx::uninterruptible::nothrow ui;
	const std::lock_guard<decltype(mutex)> lock{mutex};

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': wfile:%p fd:%d append:%p bytes:%zu",
		d.name,
		this,
		int(fd),
		data(s),
		size(s)
	};
	#endif

	const const_buffer buf
	{
		data(s), size(s)
	};

	fs::append(fd, buf);
	return Status::OK();
}
catch(const fs::error &e)
{
	log::error
	{
		log, "'%s': wfile:%p fd:%d append:%p size:%zu :%s",
		d.name,
		this,
		int(fd),
		data(s),
		size(s),
		e.what()
	};

	return error_to_status{e};
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "'%s': wfile:%p fd:%d append:%p size:%zu :%s",
		d.name,
		this,
		int(fd),
		data(s),
		size(s),
		e.what()
	};

	return error_to_status{e};
}

rocksdb::Status
ircd::db::database::env::writable_file::PositionedAppend(const Slice &s,
                                                         uint64_t offset)
noexcept try
{
	const ctx::uninterruptible::nothrow ui;
	const std::lock_guard<decltype(mutex)> lock{mutex};

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		"'%s': wfile:%p fd:%d append:%p bytes:%zu offset:%lu",
		d.name,
		this,
		int(fd),
		data(s),
		size(s),
		offset
	};
	#endif

	assert(0);
	return Status::NotSupported();

	const const_buffer buf
	{
		data(s), size(s)
	};

	fs::append(fd, buf, offset);
	return Status::OK();
}
catch(const fs::error &e)
{
	log::error
	{
		log, "'%s': wfile:%p fd:%d append:%p size:%zu offset:%zu :%s",
		d.name,
		this,
		int(fd),
		data(s),
		size(s),
		offset,
		e.what()
	};

	return error_to_status{e};
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "'%s': wfile:%p fd:%d append:%p size:%zu offset:%lu :%s",
		d.name,
		this,
		int(fd),
		data(s),
		size(s),
		offset,
		e.what()
	};

	return error_to_status{e};
}

rocksdb::Status
ircd::db::database::env::writable_file::Truncate(uint64_t size)
noexcept try
{
	const ctx::uninterruptible::nothrow ui;
	const std::lock_guard<decltype(mutex)> lock{mutex};

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		"'%s': wfile:%p fd:%d truncate to %lu bytes",
		d.name,
		this,
		int(fd),
		size
	};
	#endif

	fs::truncate(fd, size);
	return Status::OK();
}
catch(const fs::error &e)
{
	log::error
	{
		log, "'%s': wfile:%p fd:%d truncate to %lu bytes :%s",
		d.name,
		this,
		int(fd),
		size,
		e.what()
	};

	return error_to_status{e};
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "'%s': wfile:%p fd:%d truncate to %lu bytes :%s",
		d.name,
		this,
		int(fd),
		size,
		e.what()
	};

	return error_to_status{e};
}

bool
ircd::db::database::env::writable_file::IsSyncThreadSafe()
const noexcept try
{
	return true;
}
catch(...)
{
	return false;
}

void
ircd::db::database::env::writable_file::SetIOPriority(Env::IOPriority prio)
noexcept
{
	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': wfile:%p set IO prio to %s",
		d.name,
		this,
		reflect(prio)
	};
	#endif

	this->prio = prio;
}

rocksdb::Env::IOPriority
ircd::db::database::env::writable_file::GetIOPriority()
noexcept
{
	return prio;
}

uint64_t
ircd::db::database::env::writable_file::GetFileSize()
noexcept try
{
	const ctx::uninterruptible::nothrow ui;
	const std::lock_guard<decltype(mutex)> lock{mutex};

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': wfile:%p fd:%d get file size",
		d.name,
		this,
		int(fd)
	};
	#endif

	return fs::size(fd);
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "'%s': wfile:%p fd:%d get file size :%s",
		d.name,
		this,
		int(fd),
		e.what()
	};

	return 0;
}

size_t
ircd::db::database::env::writable_file::GetUniqueId(char *const id,
                                                    size_t max_size)
const noexcept try
{
	const ctx::uninterruptible::nothrow ui;

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		"'%s': wfile:%p get unique id:%p max_size:%zu",
		d.name,
		this,
		id,
		max_size
	};
	#endif

	const mutable_buffer buf
	{
		id, max_size
	};

	return size(fs::uuid(fd, buf));
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "'%s': wfile:%p get unique id :%s",
		d.name,
		this,
		e.what()
	};

	return 0;
}

rocksdb::Status
ircd::db::database::env::writable_file::InvalidateCache(size_t offset,
                                                        size_t length)
noexcept try
{
	const ctx::uninterruptible::nothrow ui;
	const std::lock_guard<decltype(mutex)> lock{mutex};

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': wfile:%p fd:%d invalidate cache offset:%zu length:%zu",
		d.name,
		this,
		int(fd),
		offset,
		length
	};
	#endif

	//TODO: ircd::fs::
	//syscall(::posix_fadvise, fd, offset, length, FADV_DONTNEED);
	return Status::OK();
}
catch(const fs::error &e)
{
	log::error
	{
		log, "'%s': wfile:%p fd:%d invalidate cache offset:%zu length:%zu",
		d.name,
		this,
		int(fd),
		offset,
		length
	};

	return error_to_status{e};
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "'%s': wfile:%p fd:%d invalidate cache offset:%zu length:%zu",
		d.name,
		this,
		int(fd),
		offset,
		length
	};

	return error_to_status{e};
}

void
ircd::db::database::env::writable_file::GetPreallocationStatus(size_t *const block_size,
                                                               size_t *const last_allocated_block)
noexcept
{
	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': wfile:%p get preallocation block_size:%p last_block:%p",
		d.name,
		this,
		block_size,
		last_allocated_block
	};
	#endif

	*block_size = this->preallocation_block_size;
	*last_allocated_block = this->preallocation_last_block;
}

void
ircd::db::database::env::writable_file::SetPreallocationBlockSize(size_t size)
noexcept
{
	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': wfile:%p set preallocation block size:%zu",
		d.name,
		this,
		size
	};
	#endif

	this->preallocation_block_size = size;
}

void
ircd::db::database::env::writable_file::PrepareWrite(size_t offset,
                                                     size_t length)
noexcept
{
	const ctx::uninterruptible::nothrow ui;
	const std::lock_guard<decltype(mutex)> lock{mutex};

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': wfile:%p prepare write offset:%zu length:%zu",
		d.name,
		this,
		offset,
		length
	};
	#endif
}

rocksdb::Status
ircd::db::database::env::writable_file::Allocate(uint64_t offset,
                                                 uint64_t length)
noexcept try
{
	const ctx::uninterruptible::nothrow ui;
	const std::lock_guard<decltype(mutex)> lock{mutex};

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': wfile:%p fd:%d allocate offset:%lu length:%lu%s",
		d.name,
		this,
		int(fd),
		offset,
		length,
		env_opts.fallocate_with_keep_size? " KEEP_SIZE" : ""
	};
	#endif

	fs::write_opts wopts;
	wopts.offset = offset;
	wopts.keep_size = env_opts.fallocate_with_keep_size;

	fs::allocate(fd, length, wopts);
	this->preallocation_last_block = offset;
	return Status::OK();
}
catch(const fs::error &e)
{
	log::error
	{
		log, "'%s': wfile:%p fd:%d allocate offset:%zu length:%zu :%s",
		d.name,
		this,
		int(fd),
		offset,
		length,
		e.what()
	};

	return error_to_status{e};
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "'%s': wfile:%p fd:%d allocate offset:%zu length:%zu :%s",
		d.name,
		this,
		int(fd),
		offset,
		length,
		e.what()
	};

	return error_to_status{e};
}

//
// sequential_file
//

decltype(ircd::db::database::env::sequential_file::default_opts)
ircd::db::database::env::sequential_file::default_opts{[]
{
	ircd::fs::fd::opts ret{std::ios_base::in};
	return ret;
}()};

ircd::db::database::env::sequential_file::sequential_file(database *const &d,
                                                          const std::string &name,
                                                          const EnvOptions &env_opts)
try
:d
{
	*d
}
,opts{[&env_opts]
{
	fs::fd::opts ret{default_opts};
	ret.direct = env_opts.use_direct_reads;
	return ret;
}()}
,fd
{
	name, this->opts
}
,offset
{
	0
}
{
	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': opened seqfile:%p fd:%d '%s'",
		d->name,
		this,
		int(fd),
		name
	};
	#endif
}
catch(const std::exception &e)
{
	log::error
	{
		log, "'%s': opening seqfile:%p `%s' :%s",
		d->name,
		this,
		name,
		e.what()
	};
}

ircd::db::database::env::sequential_file::~sequential_file()
noexcept
{
	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': close seqfile:%p fd:%d",
		d.name,
		this,
		int(fd)
	};
	#endif
}

rocksdb::Status
ircd::db::database::env::sequential_file::Read(size_t length,
                                               Slice *const result,
                                               char *const scratch)
noexcept try
{
	const ctx::uninterruptible::nothrow ui;

	assert(result);
	assert(scratch);
	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': seqfile:%p read:%p offset:%zu length:%zu scratch:%p",
		d.name,
		this,
		result,
		offset,
		length,
		scratch
	};
	#endif

	const mutable_buffer buf
	{
		scratch, length
	};

	const auto read
	{
		fs::read(fd, buf, offset)
	};

	*result = slice(read);
	offset += length;
	return Status::OK();
}
catch(const fs::error &e)
{
	log::error
	{
		log, "'%s': seqfile:%p read:%p offset:%zu length:%zu scratch:%p :%s",
		d.name,
		this,
		result,
		offset,
		length,
		scratch,
		e.what()
	};

	return error_to_status{e};
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "'%s': seqfile:%p read:%p offset:%zu length:%zu scratch:%p :%s",
		d.name,
		this,
		result,
		offset,
		length,
		scratch,
		e.what()
	};

	return error_to_status{e};
}

rocksdb::Status
ircd::db::database::env::sequential_file::PositionedRead(uint64_t offset,
                                                         size_t length,
                                                         Slice *const result,
                                                         char *const scratch)
noexcept try
{
	const ctx::uninterruptible::nothrow ui;

	assert(result);
	assert(scratch);
	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': seqfile:%p positioned read:%p offset:%zu length:%zu scratch:%p",
		d.name,
		this,
		result,
		offset,
		length,
		scratch
	};
	#endif

	const mutable_buffer buf
	{
		scratch, length
	};

	const auto read
	{
		fs::read(fd, buf, offset)
	};

	*result = slice(read);
	return Status::OK();
}
catch(const fs::error &e)
{
	log::error
	{
		log, "'%s': seqfile:%p positioned read:%p offset:%zu length:%zu scratch:%p :%s",
		d.name,
		this,
		result,
		offset,
		length,
		scratch,
		e.what()
	};

	return error_to_status{e};
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "'%s': seqfile:%p positioned read:%p offset:%zu length:%zu scratch:%p :%s",
		d.name,
		this,
		result,
		offset,
		length,
		scratch,
		e.what()
	};

	return error_to_status{e};
}

rocksdb::Status
ircd::db::database::env::sequential_file::Skip(uint64_t size)
noexcept
{
	const ctx::uninterruptible::nothrow ui;

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		"'%s': seqfile:%p offset:zu skip:%zu",
		d.name,
		this,
		offset,
		size
	};
	#endif

	offset += size;
	return Status::OK();
}

rocksdb::Status
ircd::db::database::env::sequential_file::InvalidateCache(size_t offset,
                                                          size_t length)
noexcept try
{
	ctx::uninterruptible::nothrow ui;

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		"'%s': seqfile:%p invalidate cache offset:%zu length:%zu",
		d.name,
		this,
		offset,
		length
	};
	#endif

	return Status::OK();
}
catch(const fs::error &e)
{
	log::error
	{
		"'%s': seqfile:%p invalidate cache offset:%zu length:%zu :%s",
		d.name,
		this,
		offset,
		length,
		e.what()
	};

	return error_to_status{e};
}
catch(const std::exception &e)
{
	log::critical
	{
		"'%s': seqfile:%p invalidate cache offset:%zu length:%zu :%s",
		d.name,
		this,
		offset,
		length,
		e.what()
	};

	return error_to_status{e};
}

bool
ircd::db::database::env::sequential_file::use_direct_io()
const noexcept
{
	return opts.direct;
}

size_t
ircd::db::database::env::sequential_file::GetRequiredBufferAlignment()
const noexcept
{
	const auto ret
	{
		fs::block_size(fd)
	};

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		"'%s': seqfile:%p fd:%d required alignment (logical block size) %zu",
		d.name,
		this,
		int(fd),
		ret
	};
	#endif

	return ret;
}

//
// random_access_file
//

decltype(ircd::db::database::env::random_access_file::default_opts)
ircd::db::database::env::random_access_file::default_opts{[]
{
	ircd::fs::fd::opts ret{std::ios_base::in};
	ret.direct = true;
	return ret;
}()};

ircd::db::database::env::random_access_file::random_access_file(database *const &d,
                                                                const std::string &name,
                                                                const EnvOptions &env_opts)
try
:d
{
	*d
}
,opts{[&env_opts]
{
	fs::fd::opts ret{default_opts};
	ret.direct = env_opts.use_direct_reads;
	return ret;
}()}
,fd
{
	name, this->opts
}
{
	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': opened rfile:%p fd:%d '%s'",
		d->name,
		this,
		int(fd),
		name
	};
	#endif
}
catch(const std::exception &e)
{
	log::error
	{
		log, "'%s': opening rfile:%p `%s' :%s",
		d->name,
		this,
		name,
		e.what()
	};
}

ircd::db::database::env::random_access_file::~random_access_file()
noexcept
{
	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': close rfile:%p fd:%d",
		d.name,
		this,
		int(fd)
	};
	#endif
}

rocksdb::Status
ircd::db::database::env::random_access_file::Prefetch(uint64_t offset,
                                                      size_t length)
noexcept try
{
	const ctx::uninterruptible::nothrow ui;

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': rfile:%p prefetch offset:%zu length:%zu",
		d.name,
		this,
		offset,
		length
	};
	#endif

	fs::prefetch(fd, length, offset);
	return Status::OK();
}
catch(const fs::error &e)
{
	return error_to_status{e};
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "'%s': rfile:%p prefetch offset:%zu length:%zu :%s",
		d.name,
		this,
		offset,
		length,
		e.what()
	};

	return error_to_status{e};
}

rocksdb::Status
ircd::db::database::env::random_access_file::Read(uint64_t offset,
                                                  size_t length,
                                                  Slice *const result,
                                                  char *const scratch)
const noexcept try
{
	const ctx::uninterruptible::nothrow ui;

	assert(result);
	assert(scratch);
	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': rfile:%p read:%p offset:%zu length:%zu scratch:%p",
		d.name,
		this,
		result,
		offset,
		length,
		scratch
	};
	#endif

	const mutable_buffer buf
	{
		scratch, length
	};

	const auto read
	{
		fs::read(fd, buf, offset)
	};

	*result = slice(read);
	return Status::OK();
}
catch(const fs::error &e)
{
	log::error
	{
		log, "'%s': rfile:%p read:%p offset:%zu length:%zu scratch:%p :%s",
		d.name,
		this,
		result,
		offset,
		length,
		scratch,
		e.what()
	};

	return error_to_status{e};
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "'%s': rfile:%p read:%p offset:%zu length:%zu scratch:%p :%s",
		d.name,
		this,
		result,
		offset,
		length,
		scratch,
		e.what()
	};

	return error_to_status{e};
}

rocksdb::Status
ircd::db::database::env::random_access_file::InvalidateCache(size_t offset,
                                                             size_t length)
noexcept
{
	const ctx::uninterruptible::nothrow ui;

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': rfile:%p invalidate cache offset:%zu length:%zu",
		d.name,
		this,
		offset,
		length
	};
	#endif

	return Status::OK();
}

size_t
ircd::db::database::env::random_access_file::GetUniqueId(char *const id,
                                                         size_t max_size)
const noexcept try
{
	const ctx::uninterruptible::nothrow ui;

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': rfile:%p get unique id:%p max_size:%zu",
		d.name,
		this,
		id,
		max_size
	};
	#endif

	const mutable_buffer buf
	{
		id, max_size
	};

	return size(fs::uuid(fd, buf));
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "'%s': rfile:%p GetUniqueId id:%p max_size:%zu :%s",
		d.name,
		this,
		id,
		max_size,
		e.what()
	};

	return 0;
}

void
ircd::db::database::env::random_access_file::Hint(AccessPattern pattern)
noexcept
{
	const ctx::uninterruptible::nothrow ui;

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': rfile:%p hint %s",
		d.name,
		this,
		reflect(pattern)
	};
	#endif
}

bool
ircd::db::database::env::random_access_file::use_direct_io()
const noexcept
{
	return opts.direct;
}

size_t
ircd::db::database::env::random_access_file::GetRequiredBufferAlignment()
const noexcept
{
	const auto ret
	{
		fs::block_size(fd)
	};

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		"'%s': rfile:%p fd:%d required alignment (logical block size) %zu",
		d.name,
		this,
		int(fd),
		ret
	};
	#endif

	return ret;
}

//
// random_rw_file
//

decltype(ircd::db::database::env::random_rw_file::default_opts)
ircd::db::database::env::random_rw_file::default_opts{[]
{
	ircd::fs::fd::opts ret
	{
		std::ios_base::in | std::ios_base::out
	};

	return ret;
}()};

ircd::db::database::env::random_rw_file::random_rw_file(database *const &d,
                                                        const std::string &name,
                                                        const EnvOptions &opts)
try
:d
{
	*d
}
,opts{[&opts]
{
	fs::fd::opts ret{default_opts};
	ret.direct = opts.use_direct_reads && opts.use_direct_writes;
	return ret;
}()}
,fd
{
	name, this->opts
}
{
	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': opened rwfile:%p fd:%d '%s'",
		d->name,
		this,
		int(fd),
		name
	};
	#endif
}
catch(const std::exception &e)
{
	log::error
	{
		log, "'%s': opening rwfile:%p `%s' :%s",
		d->name,
		this,
		name,
		e.what()
	};
}

ircd::db::database::env::random_rw_file::~random_rw_file()
noexcept
{
	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': close rwfile:%p fd:%d '%s'",
		d.name,
		this,
		int(fd)
	};
	#endif
}

rocksdb::Status
ircd::db::database::env::random_rw_file::Close()
noexcept try
{
	ctx::uninterruptible::nothrow ui;

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': opened rwfile:%p fd:%d '%s'",
		d.name,
		this,
		int(fd)
	};
	#endif

	this->fd = fs::fd{};
	return Status::OK();
}
catch(const fs::error &e)
{
	log::error
	{
		"'%s': rwfile:%p close :%s",
		d.name,
		this,
		e.what()
	};

	return error_to_status{e};
}
catch(const std::exception &e)
{
	log::critical
	{
		"'%s': rwfile:%p close :%s",
		d.name,
		this,
		e.what()
	};

	return error_to_status{e};
}

rocksdb::Status
ircd::db::database::env::random_rw_file::Fsync()
noexcept try
{
	ctx::uninterruptible::nothrow ui;

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': rwfile:%p fd:%d fsync",
		d.name,
		int(fd),
		this
	};
	#endif

	fs::fsync(fd);
	return Status::OK();
}
catch(const fs::error &e)
{
	log::error
	{
		"'%s': rwfile:%p fd:%d fsync :%s",
		d.name,
		this,
		int(fd),
		e.what()
	};

	return error_to_status{e};
}
catch(const std::exception &e)
{
	log::critical
	{
		"'%s': rwfile:%p fd:%d fsync :%s",
		d.name,
		this,
		int(fd),
		e.what()
	};

	return error_to_status{e};
}

rocksdb::Status
ircd::db::database::env::random_rw_file::Sync()
noexcept try
{
	ctx::uninterruptible::nothrow ui;

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': rwfile:%p fd:%d sync",
		d.name,
		int(fd),
		this
	};
	#endif

	fs::fdsync(fd);
	return Status::OK();
}
catch(const fs::error &e)
{
	log::error
	{
		"'%s': rwfile:%p fd:%d sync :%s",
		d.name,
		this,
		int(fd),
		e.what()
	};

	return error_to_status{e};
}
catch(const std::exception &e)
{
	log::critical
	{
		"'%s': rwfile:%p fd:%d sync :%s",
		d.name,
		this,
		int(fd),
		e.what()
	};

	return error_to_status{e};
}

rocksdb::Status
ircd::db::database::env::random_rw_file::Flush()
noexcept try
{
	ctx::uninterruptible::nothrow ui;

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': rwfile:%p fd:%d flush",
		d.name,
		int(fd),
		this
	};
	#endif

	fs::fdsync(fd);
	return Status::OK();
}
catch(const fs::error &e)
{
	log::error
	{
		"'%s': rwfile:%p fd:%d flush :%s",
		d.name,
		this,
		int(fd),
		e.what()
	};

	return error_to_status{e};
}
catch(const std::exception &e)
{
	log::critical
	{
		"'%s': rwfile:%p fd:%d flush :%s",
		d.name,
		this,
		int(fd),
		e.what()
	};

	return error_to_status{e};
}

rocksdb::Status
ircd::db::database::env::random_rw_file::Read(uint64_t offset,
                                              size_t length,
                                              Slice *const result,
                                              char *const scratch)
const noexcept try
{
	const ctx::uninterruptible::nothrow ui;

	assert(result);
	assert(scratch);
	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': rwfile:%p read:%p offset:%zu length:%zu scratch:%p",
		d.name,
		this,
		result,
		offset,
		length,
		scratch
	};
	#endif

	const mutable_buffer buf
	{
		scratch, length
	};

	const auto read
	{
		fs::read(fd, buf, offset)
	};

	*result = slice(read);
	return Status::OK();
}
catch(const fs::error &e)
{
	log::error
	{
		log, "'%s': rwfile:%p read:%p offset:%zu length:%zu scratch:%p :%s",
		d.name,
		this,
		result,
		offset,
		length,
		scratch,
		e.what()
	};

	return error_to_status{e};
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "'%s': rwfile:%p read:%p offset:%zu length:%zu scratch:%p :%s",
		d.name,
		this,
		result,
		offset,
		length,
		scratch,
		e.what()
	};

	return error_to_status{e};
}

rocksdb::Status
ircd::db::database::env::random_rw_file::Write(uint64_t offset,
                                               const Slice &slice)
noexcept try
{
	const ctx::uninterruptible::nothrow ui;

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': rwfile:%p fd:%d write:%p length:%zu offset:%zu",
		d.name,
		this,
		int(fd),
		data(slice),
		size(slice),
		offset
	};
	#endif

	const const_buffer buf
	{
		data(slice), size(slice)
	};

	const auto read
	{
		fs::write(fd, buf, offset)
	};

	return Status::OK();
}
catch(const fs::error &e)
{
	log::error
	{
		log, "'%s': rwfile:%p fd:%d write:%p length:%zu offset:%zu",
		d.name,
		this,
		int(fd),
		data(slice),
		size(slice),
		offset
	};

	return error_to_status{e};
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "'%s': rwfile:%p fd:%d write:%p length:%zu offset:%zu",
		d.name,
		this,
		int(fd),
		data(slice),
		size(slice),
		offset
	};

	return error_to_status{e};
}

bool
ircd::db::database::env::random_rw_file::use_direct_io()
const noexcept
{
	return opts.direct;
}

size_t
ircd::db::database::env::random_rw_file::GetRequiredBufferAlignment()
const noexcept
{
	const auto ret
	{
		fs::block_size(fd)
	};

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		"'%s': rwfile:%p fd:%d required alignment (logical block size) %zu",
		d.name,
		this,
		int(fd),
		ret
	};
	#endif

	return ret;
}

//
// directory
//

ircd::db::database::env::directory::directory(database *const &d,
                                              const std::string &name,
                                              std::unique_ptr<Directory> defaults)
:d{*d}
,defaults{std::move(defaults)}
{
}

ircd::db::database::env::directory::~directory()
noexcept
{
}

rocksdb::Status
ircd::db::database::env::directory::Fsync()
noexcept
{
	#ifdef RB_DEBUG_DB_ENV
	log.debug("'%s': directory:%p fsync",
	          d.name,
	          this);
	#endif

	return defaults->Fsync();
}

//
// file_lock
//

ircd::db::database::env::file_lock::file_lock(database *const &d)
:d{*d}
{
}

ircd::db::database::env::file_lock::~file_lock()
noexcept
{
}

//
// rocksdb::port (EXPERIMENTAL)
//

#ifdef IRCD_DB_PORT

//
// Mutex
//

static_assert
(
	sizeof(rocksdb::port::Mutex) <= sizeof(pthread_mutex_t) + 1,
	"link-time punning of our structure won't work if the structure is larger "
	"than the one rocksdb has assumed space for."
);

rocksdb::port::Mutex::Mutex()
{
	#ifdef RB_DEBUG_DB_PORT_
	if(unlikely(!ctx::current))
		return;

	log::debug
	{
		db::log, "mutex %lu %p CTOR", ctx::id(), this
	};
	#endif
}

rocksdb::port::Mutex::Mutex(bool adaptive)
:Mutex{}
{
}

rocksdb::port::Mutex::~Mutex()
{
	#ifdef RB_DEBUG_DB_PORT_
	if(unlikely(!ctx::current))
		return;

	log::debug
	{
		db::log, "mutex %lu %p DTOR", ctx::id(), this
	};
	#endif
}

void
rocksdb::port::Mutex::Lock()
{
	if(unlikely(!ctx::current))
		return;

	#ifdef RB_DEBUG_DB_PORT
	log::debug
	{
		db::log, "mutex %lu %p LOCK", ctx::id(), this
	};
	#endif

	mu.lock();
}

void
rocksdb::port::Mutex::Unlock()
{
	if(unlikely(!ctx::current))
		return;

	#ifdef RB_DEBUG_DB_PORT
	log::debug
	{
		db::log, "mutex %lu %p UNLOCK", ctx::id(), this
	};
	#endif

	assert(mu.locked());
	mu.unlock();
}

void
rocksdb::port::Mutex::AssertHeld()
{
	if(unlikely(!ctx::current))
		return;

	assert(mu.locked());
}

//
// RWMutex
//

static_assert
(
	sizeof(rocksdb::port::RWMutex) <= sizeof(pthread_rwlock_t),
	"link-time punning of our structure won't work if the structure is larger "
	"than the one rocksdb has assumed space for."
);

rocksdb::port::RWMutex::RWMutex()
{
	#ifdef RB_DEBUG_DB_PORT_
	if(unlikely(!ctx::current))
		return;

	log::debug
	{
		db::log, "shared_mutex %lu %p CTOR", ctx::id(), this
	};
	#endif
}

rocksdb::port::RWMutex::~RWMutex()
{
	#ifdef RB_DEBUG_DB_PORT_
	if(unlikely(!ctx::current))
		return;

	log::debug
	{
		db::log, "shared_mutex %lu %p DTOR", ctx::id(), this
	};
	#endif
}

void
rocksdb::port::RWMutex::ReadLock()
{
	if(unlikely(!ctx::current))
		return;

	#ifdef RB_DEBUG_DB_PORT
	log::debug
	{
		db::log, "shared_mutex %lu %p LOCK SHARED", ctx::id(), this
	};
	#endif

	assert_main_thread();
	mu.lock_shared();
}

void
rocksdb::port::RWMutex::WriteLock()
{
	if(unlikely(!ctx::current))
		return;

	#ifdef RB_DEBUG_DB_PORT
	log::debug
	{
		db::log, "shared_mutex %lu %p LOCK", ctx::id(), this
	};
	#endif

	assert_main_thread();
	mu.lock();
}

void
rocksdb::port::RWMutex::ReadUnlock()
{
	if(unlikely(!ctx::current))
		return;

	#ifdef RB_DEBUG_DB_PORT
	log::debug
	{
		db::log, "shared_mutex %lu %p UNLOCK SHARED", ctx::id(), this
	};
	#endif

	assert_main_thread();
	mu.unlock_shared();
}

void
rocksdb::port::RWMutex::WriteUnlock()
{
	if(unlikely(!ctx::current))
		return;

	#ifdef RB_DEBUG_DB_PORT
	log::debug
	{
		db::log, "shared_mutex %lu %p UNLOCK", ctx::id(), this
	};
	#endif

	assert_main_thread();
	mu.unlock();
}

//
// CondVar
//

static_assert
(
	sizeof(rocksdb::port::CondVar) <= sizeof(pthread_cond_t) + sizeof(void *),
	"link-time punning of our structure won't work if the structure is larger "
	"than the one rocksdb has assumed space for."
);

rocksdb::port::CondVar::CondVar(Mutex *mu)
:mu{mu}
{
	#ifdef RB_DEBUG_DB_PORT_
	if(unlikely(!ctx::current))
		return;

	log::debug
	{
		db::log, "cond %lu %p %p CTOR", ctx::id(), this, mu
	};
	#endif
}

rocksdb::port::CondVar::~CondVar()
{
	#ifdef RB_DEBUG_DB_PORT_
	if(unlikely(!ctx::current))
		return;

	log::debug
	{
		db::log, "cond %lu %p %p DTOR", ctx::id(), this, mu
	};
	#endif
}

void
rocksdb::port::CondVar::Wait()
{
	if(unlikely(!ctx::current))
		return;

	#ifdef RB_DEBUG_DB_PORT
	log::debug
	{
		db::log, "cond %lu %p %p WAIT", ctx::id(), this, mu
	};
	#endif

	assert(mu);
	assert_main_thread();
	mu->AssertHeld();
	cv.wait(mu->mu);
}

// Returns true if timeout occurred
bool
rocksdb::port::CondVar::TimedWait(uint64_t abs_time_us)
{
	assert(ctx::current);

	#ifdef RB_DEBUG_DB_PORT
	log::debug
	{
		db::log, "cond %lu %p %p WAIT_UNTIL %lu", ctx::id(), this, mu, abs_time_us
	};
	#endif

	assert(mu);
	assert_main_thread();
	mu->AssertHeld();
	const std::chrono::microseconds us(abs_time_us);
	const std::chrono::steady_clock::time_point tp(us);
	return cv.wait_until(mu->mu, tp) == std::cv_status::timeout;
}

void
rocksdb::port::CondVar::Signal()
{
	if(unlikely(!ctx::current))
		return;

	#ifdef RB_DEBUG_DB_PORT
	log::debug
	{
		db::log, "cond %lu %p %p NOTIFY", ctx::id(), this, mu
	};
	#endif

	assert_main_thread();
	cv.notify_one();
}

void
rocksdb::port::CondVar::SignalAll()
{
	if(unlikely(!ctx::current))
		return;

	#ifdef RB_DEBUG_DB_PORT
	log::debug
	{
		db::log, "cond %lu %p %p BROADCAST", ctx::id(), this, mu
	};
	#endif

	assert_main_thread();
	cv.notify_all();
}

#endif // IRCD_DB_PORT

///////////////////////////////////////////////////////////////////////////////
//
// db/database/env/state.h
//

///////////////////////////////////////////////////////////////////////////////
//
// db/txn.h
//

void
ircd::db::get(database &d,
              const uint64_t &seq,
              const seq_closure &closure)
{
	for_each(d, seq, seq_closure_bool{[&closure]
	(txn &txn, const uint64_t &seq)
	{
		closure(txn, seq);
		return false;
	}});
}

void
ircd::db::for_each(database &d,
                   const uint64_t &seq,
                   const seq_closure &closure)
{
	for_each(d, seq, seq_closure_bool{[&closure]
	(txn &txn, const uint64_t &seq)
	{
		closure(txn, seq);
		return true;
	}});
}

bool
ircd::db::for_each(database &d,
                   const uint64_t &seq,
                   const seq_closure_bool &closure)
{
	std::unique_ptr<rocksdb::TransactionLogIterator> tit;
	{
		const ctx::uninterruptible ui;
		throw_on_error
		{
			d.d->GetUpdatesSince(seq, &tit)
		};
	}

	assert(bool(tit));
	for(; tit->Valid(); tit->Next())
	{
		const ctx::uninterruptible ui;

		auto batchres
		{
			tit->GetBatch()
		};

		throw_on_error
		{
			tit->status()
		};

		db::txn txn
		{
			d, std::move(batchres.writeBatchPtr)
		};

		assert(bool(txn.wb));
		if(!closure(txn, batchres.sequence))
			return false;
	}

	return true;
}

std::string
ircd::db::debug(const txn &t)
{
	const rocksdb::WriteBatch &wb(t);
	return db::debug(wb);
}

void
ircd::db::for_each(const txn &t,
                   const std::function<void (const delta &)> &closure)
{
	const auto re{[&closure]
	(const delta &delta)
	{
		closure(delta);
		return true;
	}};

	const database &d(t);
	const rocksdb::WriteBatch &wb{t};
	txn::handler h{d, re};
	wb.Iterate(&h);
}

bool
ircd::db::for_each(const txn &t,
                   const std::function<bool (const delta &)> &closure)
{
	const database &d(t);
	const rocksdb::WriteBatch &wb{t};
	txn::handler h{d, closure};
	wb.Iterate(&h);
	return h._continue;
}

///
/// handler (db/database/txn.h)
///

rocksdb::Status
ircd::db::txn::handler::PutCF(const uint32_t cfid,
                              const Slice &key,
                              const Slice &val)
noexcept
{
	return callback(cfid, op::SET, key, val);
}

rocksdb::Status
ircd::db::txn::handler::DeleteCF(const uint32_t cfid,
                                 const Slice &key)
noexcept
{
	return callback(cfid, op::DELETE, key, {});
}

rocksdb::Status
ircd::db::txn::handler::DeleteRangeCF(const uint32_t cfid,
                                      const Slice &begin,
                                      const Slice &end)
noexcept
{
	return callback(cfid, op::DELETE_RANGE, begin, end);
}

rocksdb::Status
ircd::db::txn::handler::SingleDeleteCF(const uint32_t cfid,
                                       const Slice &key)
noexcept
{
	return callback(cfid, op::SINGLE_DELETE, key, {});
}

rocksdb::Status
ircd::db::txn::handler::MergeCF(const uint32_t cfid,
                                const Slice &key,
                                const Slice &value)
noexcept
{
	return callback(cfid, op::MERGE, key, value);
}

rocksdb::Status
ircd::db::txn::handler::MarkBeginPrepare()
noexcept
{
	return Status::OK();
}

rocksdb::Status
ircd::db::txn::handler::MarkEndPrepare(const Slice &xid)
noexcept
{
	return Status::OK();
}

rocksdb::Status
ircd::db::txn::handler::MarkCommit(const Slice &xid)
noexcept
{
	return Status::OK();
}

rocksdb::Status
ircd::db::txn::handler::MarkRollback(const Slice &xid)
noexcept
{
	return Status::OK();
}

rocksdb::Status
ircd::db::txn::handler::callback(const uint32_t &cfid,
                                 const op &op,
                                 const Slice &a,
                                 const Slice &b)
noexcept try
{
	auto &c{d[cfid]};
	const delta delta
	{
		op,
		db::name(c),
		slice(a),
		slice(b)
	};

	return callback(delta);
}
catch(const std::exception &e)
{
	_continue = false;
	log::critical
	{
		"txn::handler: cfid[%u]: %s", cfid, e.what()
	};

	ircd::terminate();
}

rocksdb::Status
ircd::db::txn::handler::callback(const delta &delta)
noexcept try
{
	_continue = cb(delta);
	return Status::OK();
}
catch(const std::exception &e)
{
	_continue = false;
	return Status::OK();
}

bool
ircd::db::txn::handler::Continue()
noexcept
{
	return _continue;
}

//
// txn
//

ircd::db::txn::txn(database &d)
:txn{d, opts{}}
{
}

ircd::db::txn::txn(database &d,
                   const opts &opts)
:d{&d}
,wb
{
	std::make_unique<rocksdb::WriteBatch>(opts.reserve_bytes, opts.max_bytes)
}
{
}

ircd::db::txn::txn(database &d,
                   std::unique_ptr<rocksdb::WriteBatch> &&wb)
:d{&d}
,wb{std::move(wb)}
{
}

ircd::db::txn::~txn()
noexcept
{
}

void
ircd::db::txn::operator()(const sopts &opts)
{
	assert(bool(d));
	operator()(*d, opts);
}

void
ircd::db::txn::operator()(database &d,
                          const sopts &opts)
{
	assert(bool(wb));
	commit(d, *wb, opts);
}

void
ircd::db::txn::clear()
{
	assert(bool(wb));
	wb->Clear();
}

size_t
ircd::db::txn::size()
const
{
	assert(bool(wb));
	return wb->Count();
}

size_t
ircd::db::txn::bytes()
const
{
	assert(bool(wb));
	return wb->GetDataSize();
}

bool
ircd::db::txn::has(const op &op)
const
{
	assert(bool(wb));
	switch(op)
	{
		case op::GET:              assert(0); return false;
		case op::SET:              return wb->HasPut();
		case op::MERGE:            return wb->HasMerge();
		case op::DELETE:           return wb->HasDelete();
		case op::DELETE_RANGE:     return wb->HasDeleteRange();
		case op::SINGLE_DELETE:    return wb->HasSingleDelete();
	}

	return false;
}

bool
ircd::db::txn::has(const op &op,
                   const string_view &col)
const
{
	return !for_each(*this, delta_closure_bool{[&op, &col]
	(const auto &delta)
	{
		return std::get<delta.OP>(delta) == op &&
		       std::get<delta.COL>(delta) == col;
	}});
}

void
ircd::db::txn::at(const op &op,
                  const string_view &col,
                  const delta_closure &closure)
const
{
	if(!get(op, col, closure))
		throw not_found
		{
			"db::txn::at(%s, %s): no matching delta in transaction",
			reflect(op),
			col
		};
}

bool
ircd::db::txn::get(const op &op,
                   const string_view &col,
                   const delta_closure &closure)
const
{
	return !for_each(*this, delta_closure_bool{[&op, &col, &closure]
	(const delta &delta)
	{
		if(std::get<delta.OP>(delta) == op &&
		   std::get<delta.COL>(delta) == col)
		{
			closure(delta);
			return false;
		}
		else return true;
	}});
}

bool
ircd::db::txn::has(const op &op,
                   const string_view &col,
                   const string_view &key)
const
{
	return !for_each(*this, delta_closure_bool{[&op, &col, &key]
	(const auto &delta)
	{
		return std::get<delta.OP>(delta) == op &&
		       std::get<delta.COL>(delta) == col &&
		       std::get<delta.KEY>(delta) == key;
	}});
}

void
ircd::db::txn::at(const op &op,
                  const string_view &col,
                  const string_view &key,
                  const value_closure &closure)
const
{
	if(!get(op, col, key, closure))
		throw not_found
		{
			"db::txn::at(%s, %s, %s): no matching delta in transaction",
			reflect(op),
			col,
			key
		};
}

bool
ircd::db::txn::get(const op &op,
                   const string_view &col,
                   const string_view &key,
                   const value_closure &closure)
const
{
	return !for_each(*this, delta_closure_bool{[&op, &col, &key, &closure]
	(const delta &delta)
	{
		if(std::get<delta.OP>(delta) == op &&
		   std::get<delta.COL>(delta) == col &&
		   std::get<delta.KEY>(delta) == key)
		{
			closure(std::get<delta.VAL>(delta));
			return false;
		}
		else return true;
	}});
}

ircd::db::txn::operator
ircd::db::database &()
{
	assert(bool(d));
	return *d;
}

ircd::db::txn::operator
rocksdb::WriteBatch &()
{
	assert(bool(wb));
	return *wb;
}

ircd::db::txn::operator
const ircd::db::database &()
const
{
	assert(bool(d));
	return *d;
}

ircd::db::txn::operator
const rocksdb::WriteBatch &()
const
{
	assert(bool(wb));
	return *wb;
}

//
// Checkpoint
//

ircd::db::txn::checkpoint::checkpoint(txn &t)
:t{t}
{
	assert(bool(t.wb));
	t.wb->SetSavePoint();
}

ircd::db::txn::checkpoint::~checkpoint()
noexcept
{
	const ctx::uninterruptible ui;
	if(likely(!std::uncaught_exception()))
		throw_on_error { t.wb->PopSavePoint() };
	else
		throw_on_error { t.wb->RollbackToSavePoint() };
}

ircd::db::txn::append::append(txn &t,
                              const string_view &key,
                              const json::iov &iov)
{
	std::for_each(std::begin(iov), std::end(iov), [&t, &key]
	(const auto &member)
	{
		append
		{
			t, delta
			{
				member.first,   // col
				key,            // key
				member.second   // val
			}
		};
	});
}

ircd::db::txn::append::append(txn &t,
                              const delta &delta)
{
	assert(bool(t.d));
	append(t, *t.d, delta);
}

ircd::db::txn::append::append(txn &t,
                              const row::delta &delta)
{
	assert(0);
}

ircd::db::txn::append::append(txn &t,
                              const cell::delta &delta)
{
	db::append(*t.wb, delta);
}

ircd::db::txn::append::append(txn &t,
                              column &c,
                              const column::delta &delta)
{
	db::append(*t.wb, c, delta);
}

ircd::db::txn::append::append(txn &t,
                              database &d,
                              const delta &delta)
{
	db::column c{d[std::get<1>(delta)]};
	db::append(*t.wb, c, db::column::delta
	{
		std::get<op>(delta),
		std::get<2>(delta),
		std::get<3>(delta)
	});
}

///////////////////////////////////////////////////////////////////////////////
//
// db/index.h
//

const ircd::db::gopts
ircd::db::index::applied_opts
{
	{ get::PREFIX }
};

template<class pos>
bool
ircd::db::seek(index::const_iterator_base &it,
               const pos &p)
{
	it.opts |= index::applied_opts;
	return seek(static_cast<column::const_iterator_base &>(it), p);
}
template bool ircd::db::seek<ircd::db::pos>(index::const_iterator_base &, const pos &);
template bool ircd::db::seek<ircd::string_view>(index::const_iterator_base &, const string_view &);

ircd::db::index::const_iterator
ircd::db::index::begin(const string_view &key,
                       gopts opts)
{
	const_iterator ret
	{
		c, {}, std::move(opts)
	};

	seek(ret, key);
	return ret;
}

ircd::db::index::const_iterator
ircd::db::index::end(const string_view &key,
                     gopts opts)
{
	const_iterator ret
	{
		c, {}, std::move(opts)
	};

	if(seek(ret, key))
		seek(ret, pos::END);

	return ret;
}

/// NOTE: RocksDB says they don't support reverse iteration over a prefix range
/// This means we have to forward scan to the end and then walk back! Reverse
/// iterations of an index shoud only be used for debugging and statistics! The
/// index should be ordered the way it will be primarily accessed using the
/// comparator. If it will be accessed in different directions, make another
/// index column.
ircd::db::index::const_reverse_iterator
ircd::db::index::rbegin(const string_view &key,
                        gopts opts)
{
	const_reverse_iterator ret
	{
		c, {}, std::move(opts)
	};

	if(seek(ret, key))
	{
		while(seek(ret, pos::NEXT));
		seek(ret, pos::PREV);
	}

	return ret;
}

ircd::db::index::const_reverse_iterator
ircd::db::index::rend(const string_view &key,
                      gopts opts)
{
	const_reverse_iterator ret
	{
		c, {}, std::move(opts)
	};

	if(seek(ret, key))
		seek(ret, pos::END);

	return ret;
}

//
// const_iterator
//

ircd::db::index::const_iterator &
ircd::db::index::const_iterator::operator--()
{
	if(likely(bool(*this)))
		seek(*this, pos::PREV);
	else
		seek(*this, pos::BACK);

	return *this;
}

ircd::db::index::const_iterator &
ircd::db::index::const_iterator::operator++()
{
	if(likely(bool(*this)))
		seek(*this, pos::NEXT);
	else
		seek(*this, pos::FRONT);

	return *this;
}

ircd::db::index::const_reverse_iterator &
ircd::db::index::const_reverse_iterator::operator--()
{
	if(likely(bool(*this)))
		seek(*this, pos::NEXT);
	else
		seek(*this, pos::FRONT);

	return *this;
}

ircd::db::index::const_reverse_iterator &
ircd::db::index::const_reverse_iterator::operator++()
{
	if(likely(bool(*this)))
		seek(*this, pos::PREV);
	else
		seek(*this, pos::BACK);

	return *this;
}

const ircd::db::index::const_iterator_base::value_type &
ircd::db::index::const_iterator_base::operator*()
const
{
	const auto &prefix
	{
		describe(*c).prefix
	};

	// Fetch the full value like a standard column first
	column::const_iterator_base::operator*();
	string_view &key{val.first};

	// When there's no prefixing this index column is just
	// like a normal column. Otherwise, we remove the prefix
	// from the key the user will end up seeing.
	if(prefix.has && prefix.has(key))
	{
		const auto &first(prefix.get(key));
		const auto &second(key.substr(first.size()));
		key = second;
	}

	return val;
}

const ircd::db::index::const_iterator_base::value_type *
ircd::db::index::const_iterator_base::operator->()
const
{
	return &this->operator*();
}

///////////////////////////////////////////////////////////////////////////////
//
// db/cell.h
//

uint64_t
ircd::db::sequence(const cell &c)
{
	const database::snapshot &ss(c);
	return sequence(database::snapshot(c));
}

const std::string &
ircd::db::name(const cell &c)
{
	return name(c.c);
}

void
ircd::db::write(const cell::delta &delta,
                const sopts &sopts)
{
	write(&delta, &delta + 1, sopts);
}

void
ircd::db::write(const sopts &sopts,
                const std::initializer_list<cell::delta> &deltas)
{
	write(deltas, sopts);
}

void
ircd::db::write(const std::initializer_list<cell::delta> &deltas,
                const sopts &sopts)
{
	write(std::begin(deltas), std::end(deltas), sopts);
}

void
ircd::db::write(const cell::delta *const &begin,
                const cell::delta *const &end,
                const sopts &sopts)
{
	if(begin == end)
		return;

	// Find the database through one of the cell's columns. cell::deltas
	// may come from different columns so we do nothing else with this.
	auto &front(*begin);
	column &c(std::get<cell *>(front)->c);
	database &d(c);

	rocksdb::WriteBatch batch;
	std::for_each(begin, end, [&batch]
	(const cell::delta &delta)
	{
		append(batch, delta);
	});

	commit(d, batch, sopts);
}

template<class pos>
bool
ircd::db::seek(cell &c,
               const pos &p,
               gopts opts)
{
	column &cc(c);
	database::column &dc(cc);

	if(!opts.snapshot)
		opts.snapshot = c.ss;

	const auto ropts(make_opts(opts));
	return seek(dc, p, ropts, c.it);
}
template bool ircd::db::seek<ircd::db::pos>(cell &, const pos &, gopts);
template bool ircd::db::seek<ircd::string_view>(cell &, const string_view &, gopts);

// Linkage for incomplete rocksdb::Iterator
ircd::db::cell::cell()
{
}

ircd::db::cell::cell(database &d,
                     const string_view &colname,
                     gopts opts)
:cell
{
	column(d[colname]), std::unique_ptr<rocksdb::Iterator>{}, std::move(opts)
}
{
}

ircd::db::cell::cell(database &d,
                     const string_view &colname,
                     const string_view &index,
                     gopts opts)
:cell
{
	column(d[colname]), index, std::move(opts)
}
{
}

ircd::db::cell::cell(column column,
                     const string_view &index,
                     gopts opts)
:c{std::move(column)}
,ss{opts.snapshot}
,it
{
	!index.empty()?
		seek(this->c, index, opts):
		std::unique_ptr<rocksdb::Iterator>{}
}
{
	if(bool(this->it))
		if(!valid_eq(*this->it, index))
			this->it.reset();
}

ircd::db::cell::cell(column column,
                     const string_view &index,
                     std::unique_ptr<rocksdb::Iterator> it,
                     gopts opts)
:c{std::move(column)}
,ss{opts.snapshot}
,it{std::move(it)}
{
	if(index.empty())
		return;

	seek(*this, index, opts);
	if(!valid_eq(*this->it, index))
		this->it.reset();
}

ircd::db::cell::cell(column column,
                     std::unique_ptr<rocksdb::Iterator> it,
                     gopts opts)
:c{std::move(column)}
,ss{std::move(opts.snapshot)}
,it{std::move(it)}
{
}

// Linkage for incomplete rocksdb::Iterator
ircd::db::cell::cell(cell &&o)
noexcept
:c{std::move(o.c)}
,ss{std::move(o.ss)}
,it{std::move(o.it)}
{
}

// Linkage for incomplete rocksdb::Iterator
ircd::db::cell &
ircd::db::cell::operator=(cell &&o)
noexcept
{
	c = std::move(o.c);
	ss = std::move(o.ss);
	it = std::move(o.it);

	return *this;
}

// Linkage for incomplete rocksdb::Iterator
ircd::db::cell::~cell()
noexcept
{
}

bool
ircd::db::cell::load(const string_view &index,
                     gopts opts)
{
	database &d(c);
	if(valid(index) && !opts.snapshot && sequence(ss) == sequence(d))
		return true;

	if(bool(opts.snapshot))
	{
		this->it.reset();
		this->ss = std::move(opts.snapshot);
	}

	database::column &c(this->c);
	return seek(c, index, opts, this->it);
}

ircd::db::cell &
ircd::db::cell::operator=(const string_view &s)
{
	write(c, key(), s);
	return *this;
}

void
ircd::db::cell::operator()(const op &op,
                           const string_view &val,
                           const sopts &sopts)
{
	write(cell::delta{op, *this, val}, sopts);
}

ircd::db::cell::operator
string_view()
{
	return val();
}

ircd::db::cell::operator
string_view()
const
{
	return val();
}

ircd::string_view
ircd::db::cell::val()
{
	if(!valid())
		load();

	return likely(valid())? db::val(*it) : string_view{};
}

ircd::string_view
ircd::db::cell::key()
{
	if(!valid())
		load();

	return likely(valid())? db::key(*it) : string_view{};
}

ircd::string_view
ircd::db::cell::val()
const
{
	return likely(valid())? db::val(*it) : string_view{};
}

ircd::string_view
ircd::db::cell::key()
const
{
	return likely(valid())? db::key(*it) : string_view{};
}

bool
ircd::db::cell::valid()
const
{
	return bool(it) && db::valid(*it);
}

bool
ircd::db::cell::valid(const string_view &s)
const
{
	return bool(it) && db::valid_eq(*it, s);
}

bool
ircd::db::cell::valid_gt(const string_view &s)
const
{
	return bool(it) && db::valid_gt(*it, s);
}

bool
ircd::db::cell::valid_lte(const string_view &s)
const
{
	return bool(it) && db::valid_lte(*it, s);
}

///////////////////////////////////////////////////////////////////////////////
//
// db/row.h
//

void
ircd::db::del(row &row,
              const sopts &sopts)
{
	write(row::delta{op::DELETE, row}, sopts);
}

void
ircd::db::write(const row::delta &delta,
                const sopts &sopts)
{
	write(&delta, &delta + 1, sopts);
}

void
ircd::db::write(const sopts &sopts,
                const std::initializer_list<row::delta> &deltas)
{
	write(deltas, sopts);
}

void
ircd::db::write(const std::initializer_list<row::delta> &deltas,
                const sopts &sopts)
{
	write(std::begin(deltas), std::end(deltas), sopts);
}

void
ircd::db::write(const row::delta *const &begin,
                const row::delta *const &end,
                const sopts &sopts)
{
	// Count the total number of cells for this transaction.
	const auto cells
	{
		std::accumulate(begin, end, size_t(0), []
		(auto ret, const row::delta &delta)
		{
			const auto &row(std::get<row *>(delta));
			return ret += row->size();
		})
	};

	//TODO: allocator?
	std::vector<cell::delta> deltas;
	deltas.reserve(cells);

	// Compose all of the cells from all of the rows into a single txn
	std::for_each(begin, end, [&deltas]
	(const auto &delta)
	{
		const auto &op(std::get<op>(delta));
		const auto &row(std::get<row *>(delta));
		std::for_each(std::begin(*row), std::end(*row), [&deltas, &op]
		(auto &cell)
		{
			// For operations like DELETE which don't require a value in
			// the delta, we can skip a potentially expensive load of the cell.
			const auto value
			{
				value_required(op)? cell.val() : string_view{}
			};

			deltas.emplace_back(op, cell, value);
		});
	});

	// Commitment
	write(&deltas.front(), &deltas.front() + deltas.size(), sopts);
}

size_t
ircd::db::seek(row &r,
               const pos &p)
{
	// This frame can't be interrupted because it may have requests
	// pending in the request pool which must synchronize back here.
	const ctx::uninterruptible ui;

	#ifdef RB_DEBUG_DB_SEEK
	const ircd::timer timer;
	#endif

	size_t ret{0};
	ctx::latch latch{r.size()};
	for(auto &cell : r) request([&latch, &ret, &cell, &p]
	{
		ret += bool(seek(cell, p));
		latch.count_down();
	});

	latch.wait();

	#ifdef RB_DEBUG_DB_SEEK
	const column &c(r[0]);
	const database &d(c);
	log::debug
	{
		log, "'%s' %lu:%lu '%s' row SEEK POS %zu of %zu in %ld$us",
		name(d),
		sequence(d),
		sequence(r[0]),
		name(c),
		ret,
		r.size(),
		timer.at<microseconds>().count()
	};
	#endif

	assert(ret <= r.size());
	return ret;
}

size_t
ircd::db::seek(row &r,
               const string_view &key)
{
	// This frame can't be interrupted because it may have requests
	// pending in the request pool which must synchronize back here.
	const ctx::uninterruptible ui;

	#ifdef RB_DEBUG_DB_SEEK
	const ircd::timer timer;
	#endif

	size_t ret{0};
	ctx::latch latch{r.size()};
	const auto closure{[&latch, &ret, &key]
	(auto &cell)
	{
		ret += bool(seek(cell, key));
		latch.count_down();
	}};

	for(auto &cell : r)
	{
		db::column &column(cell);
		if(!exists(cache(column), key))
			request([&closure, &cell]
			{
				closure(cell);
			});
		else
			closure(cell);
	}

	latch.wait();

	#ifdef RB_DEBUG_DB_SEEK
	const column &c(r[0]);
	const database &d(c);
	log::debug
	{
		log, "'%s' %lu:%lu '%s' row SEEK KEY %zu of %zu in %ld$us",
		name(d),
		sequence(d),
		sequence(r[0]),
		name(c),
		ret,
		r.size(),
		timer.at<microseconds>().count()
	};
	#endif

	assert(ret <= r.size());
	return ret;
}

//
// row
//

ircd::db::row::row(database &d,
                   const string_view &key,
                   const vector_view<const string_view> &colnames,
                   const vector_view<cell> &buf,
                   gopts opts)
:vector_view<cell>
{
	buf.data(),
	colnames.empty()?
		d.columns.size():
		colnames.size()
}
{
	using std::end;
	using std::begin;
	using rocksdb::Iterator;
	using rocksdb::ColumnFamilyHandle;

	if(!opts.snapshot)
		opts.snapshot = database::snapshot(d);

	const rocksdb::ReadOptions options
	{
		make_opts(opts)
	};

	const size_t &column_count
	{
		vector_view<cell>::size()
	};

	database::column *colptr[column_count];
	if(colnames.empty())
		std::transform(begin(d.columns), end(d.columns), colptr, [&colnames]
		(const auto &p)
		{
			return p.get();
		});
	else
		std::transform(begin(colnames), end(colnames), colptr, [&d]
		(const auto &name)
		{
			return &d[name];
		});

	//TODO: allocator
	std::vector<ColumnFamilyHandle *> handles(column_count);
	std::transform(colptr, colptr + column_count, begin(handles), []
	(database::column *const &ptr)
	{
		return ptr->handle.get();
	});

	//TODO: does this block?
	std::vector<Iterator *> iterators;
	{
		const ctx::uninterruptible ui;
		throw_on_error
		{
			d.d->NewIterators(options, handles, &iterators)
		};
	}

	for(size_t i(0); i < this->size() && i < column_count; ++i)
	{
		std::unique_ptr<Iterator> it(iterators.at(i));
		(*this)[i] = cell { *colptr[i], std::move(it), opts };
	}

	if(key)
		seek(*this, key);
}

void
ircd::db::row::operator()(const op &op,
                          const string_view &col,
                          const string_view &val,
                          const sopts &sopts)
{
	write(cell::delta{op, (*this)[col], val}, sopts);
}

ircd::db::cell &
ircd::db::row::operator[](const string_view &column)
{
	const auto it(find(column));
	if(unlikely(it == end()))
		throw schema_error
		{
			"column '%s' not specified in the descriptor schema", column
		};

	return *it;
}

const ircd::db::cell &
ircd::db::row::operator[](const string_view &column)
const
{
	const auto it(find(column));
	if(unlikely(it == end()))
		throw schema_error
		{
			"column '%s' not specified in the descriptor schema", column
		};

	return *it;
}

ircd::db::row::iterator
ircd::db::row::find(const string_view &col)
{
	return std::find_if(std::begin(*this), std::end(*this), [&col]
	(const auto &cell)
	{
		return name(cell.c) == col;
	});
}

ircd::db::row::const_iterator
ircd::db::row::find(const string_view &col)
const
{
	return std::find_if(std::begin(*this), std::end(*this), [&col]
	(const auto &cell)
	{
		return name(cell.c) == col;
	});
}

bool
ircd::db::row::valid()
const
{
	return std::any_of(std::begin(*this), std::end(*this), []
	(const auto &cell)
	{
		return cell.valid();
	});
}

bool
ircd::db::row::valid(const string_view &s)
const
{
	return std::any_of(std::begin(*this), std::end(*this), [&s]
	(const auto &cell)
	{
		return cell.valid(s);
	});
}

///////////////////////////////////////////////////////////////////////////////
//
// db/column.h
//

std::string
ircd::db::read(column &column,
               const string_view &key,
               const gopts &gopts)
{
	std::string ret;
	const auto closure([&ret]
	(const string_view &src)
	{
		ret.assign(begin(src), end(src));
	});

	column(key, closure, gopts);
	return ret;
}

ircd::string_view
ircd::db::read(column &column,
               const string_view &key,
               const mutable_buffer &buf,
               const gopts &gopts)
{
	string_view ret;
	const auto closure([&ret, &buf]
	(const string_view &src)
	{
		ret = { data(buf), copy(buf, src) };
	});

	column(key, closure, gopts);
	return ret;
}

std::string
ircd::db::read(column &column,
               const string_view &key,
               bool &found,
               const gopts &gopts)
{
	std::string ret;
	const auto closure([&ret]
	(const string_view &src)
	{
		ret.assign(begin(src), end(src));
	});

	found = column(key, std::nothrow, closure, gopts);
	return ret;
}

ircd::string_view
ircd::db::read(column &column,
               const string_view &key,
               bool &found,
               const mutable_buffer &buf,
               const gopts &gopts)
{
	string_view ret;
	const auto closure([&buf, &ret]
	(const string_view &src)
	{
		ret = { data(buf), copy(buf, src) };
	});

	found = column(key, std::nothrow, closure, gopts);
	return ret;
}

rocksdb::Cache *
ircd::db::cache(column &column)
{
	database::column &c(column);
	return c.table_opts.block_cache.get();
}

rocksdb::Cache *
ircd::db::cache_compressed(column &column)
{
	database::column &c(column);
	return c.table_opts.block_cache_compressed.get();
}

const rocksdb::Cache *
ircd::db::cache(const column &column)
{
	const database::column &c(column);
	return c.table_opts.block_cache.get();
}

const rocksdb::Cache *
ircd::db::cache_compressed(const column &column)
{
	const database::column &c(column);
	return c.table_opts.block_cache_compressed.get();
}

template<>
ircd::db::prop_str
ircd::db::property(const column &column,
                   const string_view &name)
{
	std::string ret;
	database::column &c(const_cast<db::column &>(column));
	database &d(const_cast<db::column &>(column));
	if(!d.d->GetProperty(c, slice(name), &ret))
		throw not_found
		{
			"'property '%s' for column '%s' in '%s' not found.",
			name,
			db::name(column),
			db::name(d)
		};

	return ret;
}

template<>
ircd::db::prop_int
ircd::db::property(const column &column,
                   const string_view &name)
{
	uint64_t ret;
	database::column &c(const_cast<db::column &>(column));
	database &d(const_cast<db::column &>(column));
	if(!d.d->GetIntProperty(c, slice(name), &ret))
		throw not_found
		{
			"property '%s' for column '%s' in '%s' not found or not an integer.",
			name,
			db::name(column),
			db::name(d)
		};

	return ret;
}

template<>
ircd::db::prop_map
ircd::db::property(const column &column,
                   const string_view &name)
{
	std::map<std::string, std::string> ret;
	database::column &c(const_cast<db::column &>(column));
	database &d(const_cast<db::column &>(column));
	if(!d.d->GetMapProperty(c, slice(name), &ret))
		ret.emplace(std::string{name}, property<std::string>(column, name));

	return ret;
}

size_t
ircd::db::bytes(const column &column)
{
	rocksdb::ColumnFamilyMetaData cfm;
	database &d(const_cast<db::column &>(column));
	database::column &c(const_cast<db::column &>(column));
	assert(bool(c.handle));
	d.d->GetColumnFamilyMetaData(c.handle.get(), &cfm);
	return cfm.size;
}

size_t
ircd::db::file_count(const column &column)
{
	rocksdb::ColumnFamilyMetaData cfm;
	database &d(const_cast<db::column &>(column));
	database::column &c(const_cast<db::column &>(column));
	assert(bool(c.handle));
	d.d->GetColumnFamilyMetaData(c.handle.get(), &cfm);
	return cfm.file_count;
}

uint32_t
ircd::db::id(const column &column)
{
	const database::column &c(column);
	return id(c);
}

const std::string &
ircd::db::name(const column &column)
{
	const database::column &c(column);
	return name(c);
}

const ircd::db::database::descriptor &
ircd::db::describe(const column &column)
{
	const database::column &c(column);
	return describe(c);
}

void
ircd::db::sort(column &column,
               const bool &blocking)
{
	const ctx::uninterruptible ui;
	database::column &c(column);
	database &d(*c.d);

	rocksdb::FlushOptions opts;
	opts.wait = blocking;

	log::debug
	{
		log, "'%s':'%s' @%lu FLUSH (sort)",
		name(d),
		name(c),
		sequence(d)
	};

	throw_on_error
	{
		d.d->Flush(opts, c)
	};
}

void
ircd::db::compact(column &column,
                  const int &level_)
{
	database::column &c(column);
	database &d(*c.d);

	rocksdb::ColumnFamilyMetaData cfmd;
	d.d->GetColumnFamilyMetaData(c, &cfmd);
	for(const auto &level : cfmd.levels)
	{
		if(level_ != -1 && level.level != level_)
			continue;

		if(level.files.empty())
			continue;

		std::vector<std::string> files(level.files.size());
		std::transform(level.files.begin(), level.files.end(), files.begin(), []
		(auto &metadata)
		{
			return std::move(metadata.name);
		});

		// Locking the write_mutex here prevents writes during a column's
		// compaction. This is needed because if contention occurs inside
		// rocksdb we will hit some std::mutex's which do not use the
		// rocksdb::port wrapper and deadlock the process. (It is an error
		// on the part of rocksdb to directly use std::mutex rather than their
		// port wrapper).
		ctx::interruption_point();
		const std::lock_guard<decltype(write_mutex)> lock{write_mutex};
		const ctx::uninterruptible::nothrow ui;

		log::debug
		{
			log, "'%s':'%s' COMPACT level:%d files:%zu size:%zu",
			name(d),
			name(c),
			level.level,
			level.files.size(),
			level.size
		};

		rocksdb::CompactionOptions opts;
		throw_on_error
		{
			d.d->CompactFiles(opts, c, files, level.level)
		};
	}
}

void
ircd::db::compact(column &column,
                  const std::pair<string_view, string_view> &range,
                  const int &to_level)
{
	ctx::interruption_point();
	ctx::uninterruptible::nothrow ui;

	database::column &c(column);
	database &d(*c.d);

	const auto begin(slice(range.first));
	const rocksdb::Slice *const b
	{
		empty(range.first)? nullptr : &begin
	};

	const auto end(slice(range.second));
	const rocksdb::Slice *const e
	{
		empty(range.second)? nullptr : &end
	};

	log::debug
	{
		log, "'%s':'%s' @%lu COMPACT [%s, %s] to level %d",
		name(d),
		name(c),
		sequence(d),
		range.first,
		range.second,
		to_level
	};

	rocksdb::CompactRangeOptions opts;
	opts.change_level = true;
	opts.target_level = to_level;
	opts.allow_write_stall = true;
	throw_on_error
	{
		d.d->CompactRange(opts, c, b, e)
	};
}

void
ircd::db::setopt(column &column,
                 const string_view &key,
                 const string_view &val)
{
	const ctx::uninterruptible ui;
	const std::unordered_map<std::string, std::string> options
	{
		{ std::string{key}, std::string{val} }
	};

	database::column &c(column);
	database &d(c);
	throw_on_error
	{
		d.d->SetOptions(c, options)
	};
}

void
ircd::db::del(column &column,
              const string_view &key,
              const sopts &sopts)
{
	const ctx::uninterruptible ui;
	database::column &c(column);
	database &d(column);
	log::debug
	{
		log, "'%s' %lu '%s' DELETE key(%zu B)",
		name(d),
		sequence(d),
		name(c),
		key.size()
	};

	auto opts(make_opts(sopts));
	throw_on_error
	{
		d.d->Delete(opts, c, slice(key))
	};
}

void
ircd::db::write(column &column,
                const string_view &key,
                const const_buffer &val,
                const sopts &sopts)
{
	const ctx::uninterruptible ui;
	database::column &c(column);
	database &d(column);
	log::debug
	{
		log, "'%s' %lu '%s' PUT key(%zu B) val(%zu B)",
		name(d),
		sequence(d),
		name(c),
		size(key),
		size(val)
	};

	auto opts(make_opts(sopts));
	throw_on_error
	{
		d.d->Put(opts, c, slice(key), slice(val))
	};
}

bool
ircd::db::cached(column &column,
                 const string_view &key,
                 const gopts &gopts)
{
	database &d(column);
	database::column &c(column);

	auto opts(make_opts(gopts));
	opts.read_tier = NON_BLOCKING;
	opts.fill_cache = false;

	std::unique_ptr<rocksdb::Iterator> it;
	if(!seek(c, key, opts, it))
		return false;

	assert(bool(it));
	return valid_eq(*it, key);
}

bool
ircd::db::has(column &column,
              const string_view &key,
              const gopts &gopts)
{
	database &d(column);
	database::column &c(column);

	const auto k(slice(key));
	auto opts(make_opts(gopts));
	opts.read_tier = NON_BLOCKING;

	// Perform a co-RP query to the filtration
	thread_local std::string discard;
	bool ret
	{
		d.d->KeyMayExist(opts, c, k, &discard, nullptr)
	};

	if(ret)
	{
		const auto it
		{
			seek(column, key, gopts)
		};

		ret = valid_eq(*it, key);
	}

	return ret;
}

//
// column
//

ircd::db::column::column(database::column &c)
:c{&c}
{
}

ircd::db::column::column(database &d,
                         const string_view &column_name)
:c{&d[column_name]}
{}

void
ircd::db::column::operator()(const delta &delta,
                             const sopts &sopts)
{
	operator()(&delta, &delta + 1, sopts);
}

void
ircd::db::column::operator()(const sopts &sopts,
                             const std::initializer_list<delta> &deltas)
{
	operator()(deltas, sopts);
}

void
ircd::db::column::operator()(const std::initializer_list<delta> &deltas,
                             const sopts &sopts)
{
	operator()(std::begin(deltas), std::end(deltas), sopts);
}

void
ircd::db::column::operator()(const delta *const &begin,
                             const delta *const &end,
                             const sopts &sopts)
{
	database &d(*this);

	rocksdb::WriteBatch batch;
	std::for_each(begin, end, [this, &batch]
	(const delta &delta)
	{
		append(batch, *this, delta);
	});

	commit(d, batch, sopts);
}

void
ircd::db::column::operator()(const string_view &key,
                             const gopts &gopts,
                             const view_closure &func)
{
	return operator()(key, func, gopts);
}

void
ircd::db::column::operator()(const string_view &key,
                             const view_closure &func,
                             const gopts &gopts)
{
	const auto it(seek(*this, key, gopts));
	valid_eq_or_throw(*it, key);
	func(val(*it));
}

bool
ircd::db::column::operator()(const string_view &key,
                             const std::nothrow_t,
                             const gopts &gopts,
                             const view_closure &func)
{
	return operator()(key, std::nothrow, func, gopts);
}

bool
ircd::db::column::operator()(const string_view &key,
                             const std::nothrow_t,
                             const view_closure &func,
                             const gopts &gopts)
{
	const auto it(seek(*this, key, gopts));
	if(!valid_eq(*it, key))
		return false;

	func(val(*it));
	return true;
}

ircd::db::cell
ircd::db::column::operator[](const string_view &key)
const
{
	return { *this, key };
}

ircd::db::column::operator
const database::descriptor &()
const
{
	return c->descriptor;
}

//
// column::const_iterator
//

namespace ircd {
namespace db   {

} // namespace db
} // namespace ircd

ircd::db::column::const_iterator
ircd::db::column::end(gopts gopts)
{
	const_iterator ret
	{
		c, {}, std::move(gopts)
	};

	seek(ret, pos::END);
	return ret;
}

ircd::db::column::const_iterator
ircd::db::column::begin(gopts gopts)
{
	const_iterator ret
	{
		c, {}, std::move(gopts)
	};

	seek(ret, pos::FRONT);
	return ret;
}

ircd::db::column::const_reverse_iterator
ircd::db::column::rend(gopts gopts)
{
	const_reverse_iterator ret
	{
		c, {}, std::move(gopts)
	};

	seek(ret, pos::END);
	return ret;
}

ircd::db::column::const_reverse_iterator
ircd::db::column::rbegin(gopts gopts)
{
	const_reverse_iterator ret
	{
		c, {}, std::move(gopts)
	};

	seek(ret, pos::BACK);
	return ret;
}

ircd::db::column::const_iterator
ircd::db::column::upper_bound(const string_view &key,
                              gopts gopts)
{
	auto it(lower_bound(key, std::move(gopts)));
	if(it && it.it->key().compare(slice(key)) == 0)
		++it;

	return it;
}

ircd::db::column::const_iterator
ircd::db::column::find(const string_view &key,
                       gopts gopts)
{
	auto it(lower_bound(key, gopts));
	if(!it || it.it->key().compare(slice(key)) != 0)
		return end(gopts);

	return it;
}

ircd::db::column::const_iterator
ircd::db::column::lower_bound(const string_view &key,
                              gopts gopts)
{
	const_iterator ret
	{
		c, {}, std::move(gopts)
	};

	seek(ret, key);
	return ret;
}

ircd::db::column::const_iterator &
ircd::db::column::const_iterator::operator--()
{
	if(likely(bool(*this)))
		seek(*this, pos::PREV);
	else
		seek(*this, pos::BACK);

	return *this;
}

ircd::db::column::const_iterator &
ircd::db::column::const_iterator::operator++()
{
	if(likely(bool(*this)))
		seek(*this, pos::NEXT);
	else
		seek(*this, pos::FRONT);

	return *this;
}

ircd::db::column::const_reverse_iterator &
ircd::db::column::const_reverse_iterator::operator--()
{
	if(likely(bool(*this)))
		seek(*this, pos::NEXT);
	else
		seek(*this, pos::FRONT);

	return *this;
}

ircd::db::column::const_reverse_iterator &
ircd::db::column::const_reverse_iterator::operator++()
{
	if(likely(bool(*this)))
		seek(*this, pos::PREV);
	else
		seek(*this, pos::BACK);

	return *this;
}

ircd::db::column::const_iterator_base::const_iterator_base(const_iterator_base &&o)
noexcept
:c{std::move(o.c)}
,opts{std::move(o.opts)}
,it{std::move(o.it)}
,val{std::move(o.val)}
{
}

ircd::db::column::const_iterator_base &
ircd::db::column::const_iterator_base::operator=(const_iterator_base &&o)
noexcept
{
	c = std::move(o.c);
	opts = std::move(o.opts);
	it = std::move(o.it);
	val = std::move(o.val);
	return *this;
}

// linkage for incmplete rocksdb::Iterator
ircd::db::column::const_iterator_base::const_iterator_base()
{
}

// linkage for incmplete rocksdb::Iterator
ircd::db::column::const_iterator_base::~const_iterator_base()
noexcept
{
}

ircd::db::column::const_iterator_base::const_iterator_base(database::column *const &c,
                                                           std::unique_ptr<rocksdb::Iterator> &&it,
                                                            gopts opts)
:c{c}
,opts{std::move(opts)}
,it{std::move(it)}
{
}

const ircd::db::column::const_iterator_base::value_type &
ircd::db::column::const_iterator_base::operator*()
const
{
	assert(it && valid(*it));
	val.first = db::key(*it);
	val.second = db::val(*it);
	return val;
}

const ircd::db::column::const_iterator_base::value_type *
ircd::db::column::const_iterator_base::operator->()
const
{
	return &operator*();
}

bool
ircd::db::column::const_iterator_base::operator!()
const
{
	return !static_cast<bool>(*this);
}

ircd::db::column::const_iterator_base::operator bool()
const
{
	if(!it)
		return false;

	if(!valid(*it))
		return false;

	return true;
}

bool
ircd::db::operator!=(const column::const_iterator_base &a, const column::const_iterator_base &b)
{
	return !(a == b);
}

bool
ircd::db::operator==(const column::const_iterator_base &a, const column::const_iterator_base &b)
{
	if(a && b)
	{
		const auto &ak(a.it->key());
		const auto &bk(b.it->key());
		return ak.compare(bk) == 0;
	}

	if(!a && !b)
		return true;

	return false;
}

bool
ircd::db::operator>(const column::const_iterator_base &a, const column::const_iterator_base &b)
{
	if(a && b)
	{
		const auto &ak(a.it->key());
		const auto &bk(b.it->key());
		return ak.compare(bk) == 1;
	}

	if(!a && b)
		return true;

	if(!a && !b)
		return false;

	assert(!a && b);
	return false;
}

bool
ircd::db::operator<(const column::const_iterator_base &a, const column::const_iterator_base &b)
{
	if(a && b)
	{
		const auto &ak(a.it->key());
		const auto &bk(b.it->key());
		return ak.compare(bk) == -1;
	}

	if(!a && b)
		return false;

	if(!a && !b)
		return false;

	assert(a && !b);
	return true;
}

template<class pos>
bool
ircd::db::seek(column::const_iterator_base &it,
               const pos &p)
{
	database::column &c(it);
	const auto ropts
	{
		make_opts(it.opts)
	};

	return seek(c, p, ropts, it.it);
}
template bool ircd::db::seek<ircd::db::pos>(column::const_iterator_base &, const pos &);
template bool ircd::db::seek<ircd::string_view>(column::const_iterator_base &, const string_view &);

///////////////////////////////////////////////////////////////////////////////
//
// merge.h
//

std::string
ircd::db::merge_operator(const string_view &key,
                         const std::pair<string_view, string_view> &delta)
{
	//ircd::json::index index{delta.first};
	//index += delta.second;
	//return index;
	assert(0);
	return {};
}

///////////////////////////////////////////////////////////////////////////////
//
// writebatch
//

void
ircd::db::append(rocksdb::WriteBatch &batch,
                 const cell::delta &delta)
{
	auto &column(std::get<cell *>(delta)->c);
	append(batch, column, column::delta
	{
		std::get<op>(delta),
		std::get<cell *>(delta)->key(),
		std::get<string_view>(delta)
	});
}

void
ircd::db::append(rocksdb::WriteBatch &batch,
                 column &column,
                 const column::delta &delta)
{
	database::column &c(column);

	const auto k(slice(std::get<1>(delta)));
	const auto v(slice(std::get<2>(delta)));
	switch(std::get<0>(delta))
	{
		case op::GET:            assert(0);                    break;
		case op::SET:            batch.Put(c, k, v);           break;
		case op::MERGE:          batch.Merge(c, k, v);         break;
		case op::DELETE:         batch.Delete(c, k);           break;
		case op::DELETE_RANGE:   batch.DeleteRange(c, k, v);   break;
		case op::SINGLE_DELETE:  batch.SingleDelete(c, k);     break;
	}
}

void
ircd::db::commit(database &d,
                 rocksdb::WriteBatch &batch,
                 const sopts &sopts)
{
	const auto opts(make_opts(sopts));
	commit(d, batch, opts);
}

void
ircd::db::commit(database &d,
                 rocksdb::WriteBatch &batch,
                 const rocksdb::WriteOptions &opts)
{
	#ifdef RB_DEBUG_DB_SEEK
	ircd::timer timer;
	#endif

	// This lock is necessary to serialize entry into rocksdb's write impl
	// otherwise there's a risk of a deadlock if their internal pthread
	// mutexes are contended. This is because a few parts of rocksdb are
	// using std::mutex directly when they ought to be using their
	// rocksdb::port wrapper.
	const std::lock_guard<decltype(write_mutex)> lock{write_mutex};
	const ctx::uninterruptible ui;

	throw_on_error
	{
		d.d->Write(opts, &batch)
	};

	#ifdef RB_DEBUG_DB_SEEK
	log::debug
	{
		log, "'%s' %lu COMMIT %s in %ld$us",
		d.name,
		sequence(d),
		debug(batch),
		timer.at<microseconds>().count()
	};
	#endif
}

std::string
ircd::db::debug(const rocksdb::WriteBatch &batch)
{
	return ircd::string(512, [&batch]
	(const mutable_buffer &ret)
	{
		return snprintf(data(ret), size(ret)+1,
		                "%d deltas; size: %zuB :%s%s%s%s%s%s%s%s%s",
		                batch.Count(),
		                batch.GetDataSize(),
		                batch.HasPut()? " PUT" : "",
		                batch.HasDelete()? " DELETE" : "",
		                batch.HasSingleDelete()? " SINGLE_DELETE" : "",
		                batch.HasDeleteRange()? " DELETE_RANGE" : "",
		                batch.HasMerge()? " MERGE" : "",
		                batch.HasBeginPrepare()? " BEGIN_PREPARE" : "",
		                batch.HasEndPrepare()? " END_PREPARE" : "",
		                batch.HasCommit()? " COMMIT" : "",
		                batch.HasRollback()? " ROLLBACK" : "");
	});
}

bool
ircd::db::has(const rocksdb::WriteBatch &wb,
              const op &op)
{
	switch(op)
	{
		case op::GET:              assert(0); return false;
		case op::SET:              return wb.HasPut();
		case op::MERGE:            return wb.HasMerge();
		case op::DELETE:           return wb.HasDelete();
		case op::DELETE_RANGE:     return wb.HasDeleteRange();
		case op::SINGLE_DELETE:    return wb.HasSingleDelete();
	}

	return false;
}

///////////////////////////////////////////////////////////////////////////////
//
// seek
//

namespace ircd::db
{
	static rocksdb::Iterator &_seek_(rocksdb::Iterator &, const pos &);
	static rocksdb::Iterator &_seek_(rocksdb::Iterator &, const string_view &);
	static rocksdb::Iterator &_seek_lower_(rocksdb::Iterator &, const string_view &);
	static rocksdb::Iterator &_seek_upper_(rocksdb::Iterator &, const string_view &);
	bool _seek(database::column &, const pos &, const rocksdb::ReadOptions &, rocksdb::Iterator &it);
	bool _seek(database::column &, const string_view &, const rocksdb::ReadOptions &, rocksdb::Iterator &it);
}

std::unique_ptr<rocksdb::Iterator>
ircd::db::seek(column &column,
               const string_view &key,
               const gopts &opts)
{
	database &d(column);
	database::column &c(column);

	std::unique_ptr<rocksdb::Iterator> ret;
	seek(c, key, opts, ret);
	return std::move(ret);
}

template<class pos>
bool
ircd::db::seek(database::column &c,
               const pos &p,
               const gopts &gopts,
               std::unique_ptr<rocksdb::Iterator> &it)
{
	auto opts
	{
		make_opts(gopts)
	};

	return seek(c, p, opts, it);
}

template<class pos>
bool
ircd::db::seek(database::column &c,
               const pos &p,
               const rocksdb::ReadOptions &opts,
               std::unique_ptr<rocksdb::Iterator> &it)
{
	const ctx::uninterruptible ui;

	if(!it)
	{
		database &d(*c.d);
		rocksdb::ColumnFamilyHandle *const &cf(c);
		it.reset(d.d->NewIterator(opts, cf));
	}

	return _seek(c, p, opts, *it);
}

bool
ircd::db::_seek(database::column &c,
                const string_view &p,
                const rocksdb::ReadOptions &opts,
                rocksdb::Iterator &it)
{
	#ifdef RB_DEBUG_DB_SEEK
	database &d(*c.d);
	const ircd::timer timer;
	#endif

	_seek_(it, p);

	#ifdef RB_DEBUG_DB_SEEK
	log::debug
	{
		log, "'%s' %lu:%lu SEEK %s in %ld$us '%s'",
		name(d),
		sequence(d),
		sequence(opts.snapshot),
		it.status().ToString(),
		timer.at<microseconds>().count(),
		name(c)
	};
	#endif

	return valid(it);
}

bool
ircd::db::_seek(database::column &c,
                const pos &p,
                const rocksdb::ReadOptions &opts,
                rocksdb::Iterator &it)
{
	#ifdef RB_DEBUG_DB_SEEK
	database &d(*c.d);
	const ircd::timer timer;
	const bool valid_it
	{
		valid(it)
	};
	#endif

	_seek_(it, p);

	#ifdef RB_DEBUG_DB_SEEK
	log::debug
	{
		log, "'%s' %lu:%lu SEEK[%s] %s -> %s in %ld$us '%s'",
		name(d),
		sequence(d),
		sequence(opts.snapshot),
		reflect(p),
		valid_it? "VALID" : "INVALID",
		it.status().ToString(),
		timer.at<microseconds>().count(),
		name(c)
	};
	#endif

	return valid(it);
}

/// Seek to entry NOT GREATER THAN key. That is, equal to or less than key
rocksdb::Iterator &
ircd::db::_seek_lower_(rocksdb::Iterator &it,
                       const string_view &sv)
{
	it.SeekForPrev(slice(sv));
	return it;
}

/// Seek to entry NOT LESS THAN key. That is, equal to or greater than key
rocksdb::Iterator &
ircd::db::_seek_upper_(rocksdb::Iterator &it,
                       const string_view &sv)
{
	it.Seek(slice(sv));
	return it;
}

/// Defaults to _seek_upper_ because it has better support from RocksDB.
rocksdb::Iterator &
ircd::db::_seek_(rocksdb::Iterator &it,
                 const string_view &sv)
{
	return _seek_upper_(it, sv);
}

rocksdb::Iterator &
ircd::db::_seek_(rocksdb::Iterator &it,
                 const pos &p)
{
	switch(p)
	{
		case pos::NEXT:     it.Next();           break;
		case pos::PREV:     it.Prev();           break;
		case pos::FRONT:    it.SeekToFirst();    break;
		case pos::BACK:     it.SeekToLast();     break;
		default:
		case pos::END:
		{
			it.SeekToLast();
			if(it.Valid())
				it.Next();

			break;
		}
	}

	return it;
}

///////////////////////////////////////////////////////////////////////////////
//
// cache.h
//

void
ircd::db::prefetch(rocksdb::Cache *const &cache,
                   column &column,
                   const string_view &key)
{
	if(cache)
		return prefetch(*cache, column, key);
}

void
ircd::db::prefetch(rocksdb::Cache &cache,
                   column &column,
                   const string_view &key)
{
	assert(0);
}

bool
ircd::db::fetch(rocksdb::Cache *const &cache,
                column &column,
                const string_view &key)
{
	return cache? fetch(*cache, column, key) : false;
}

bool
ircd::db::fetch(rocksdb::Cache &cache,
                column &column,
                const string_view &key)
{
	const db::gopts opts
	{
		// skip rocksdb inserting this into the columns cache twice.
		&cache == db::cache(column) || &cache == db::cache_compressed(column)?
			db::get::NO_CACHE:
			(enum db::get)0
	};

	const auto closure{[&cache, &key]
	(const string_view &value)
	{
		insert(cache, key, value);
	}};

	return column(key, std::nothrow, closure, opts);
}

void
ircd::db::clear(rocksdb::Cache *const &cache)
{
	if(cache)
		return clear(*cache);
}

void
ircd::db::clear(rocksdb::Cache &cache)
{
	cache.EraseUnRefEntries();
}

bool
ircd::db::remove(rocksdb::Cache *const &cache,
                 const string_view &key)
{
	return cache? remove(*cache, key) : false;
}

bool
ircd::db::remove(rocksdb::Cache &cache,
                 const string_view &key)
{
	cache.Erase(slice(key));
	return true;
}

bool
ircd::db::insert(rocksdb::Cache *const &cache,
                 const string_view &key,
                 const string_view &value)
{
	return cache? insert(*cache, key, value) : false;
}

bool
ircd::db::insert(rocksdb::Cache &cache,
                 const string_view &key,
                 const string_view &value)
{
	unique_buffer<const_buffer> buf
	{
		const_buffer{value}
	};

	return insert(cache, key, std::move(buf));
}

bool
ircd::db::insert(rocksdb::Cache *const &cache,
                 const string_view &key,
                 unique_buffer<const_buffer> value)
{
	return cache? insert(*cache, key, std::move(value)) : false;
}

bool
ircd::db::insert(rocksdb::Cache &cache,
                 const string_view &key,
                 unique_buffer<const_buffer> value)
{
	const ctx::uninterruptible ui;
	const size_t value_size
	{
		size(value)
	};

	static const auto deleter{[]
	(const rocksdb::Slice &key, void *const value)
	{
		delete[] reinterpret_cast<const char *>(value);
	}};

	// Note that because of the nullptr handle argument below, rocksdb
	// will run the deleter if the insert throws; just make sure
	// the argument execution doesn't throw after release()
	throw_on_error
	{
		cache.Insert(slice(key),
		             const_cast<char *>(data(value.release())),
		             value_size,
		             deleter,
		             nullptr)
	};

	return true;
}

void
ircd::db::for_each(const rocksdb::Cache *const &cache,
                   const cache_closure &closure)
{
	if(cache)
		for_each(*cache, closure);
}

void
ircd::db::for_each(const rocksdb::Cache &cache,
                   const cache_closure &closure)
{
	// Due to the use of the global variables which are required when using a
	// C-style callback for RocksDB, we have to make use of this function
	// exclusive for different contexts.
	thread_local ctx::mutex mutex;
	const std::lock_guard<decltype(mutex)> lock{mutex};

	thread_local rocksdb::Cache *_cache;
	_cache = const_cast<rocksdb::Cache *>(&cache);

	thread_local const cache_closure *_closure;
	_closure = &closure;

	_cache->ApplyToAllCacheEntries([]
	(void *const value_buffer, const size_t buffer_size)
	noexcept
	{
		assert(_cache);
		assert(_closure);
		const const_buffer buf
		{
			reinterpret_cast<const char *>(value_buffer), buffer_size
		};

		(*_closure)(buf);
	},
	true);
}

bool
ircd::db::exists(const rocksdb::Cache *const &cache,
                 const string_view &key)
{
	return cache? exists(*cache, key) : false;
}

bool
ircd::db::exists(const rocksdb::Cache &cache_,
                 const string_view &key)
{
	auto &cache(const_cast<rocksdb::Cache &>(cache_));
	const custom_ptr<rocksdb::Cache::Handle> handle
	{
		cache.Lookup(slice(key)), [&cache](auto *const &handle)
		{
			cache.Release(handle);
		}
	};

	return bool(handle);
}

size_t
ircd::db::usage(const rocksdb::Cache *const &cache)
{
	return cache? usage(*cache) : 0;
}

size_t
ircd::db::usage(const rocksdb::Cache &cache)
{
	return cache.GetUsage();
}

bool
ircd::db::capacity(rocksdb::Cache *const &cache,
                   const size_t &cap)
{
	if(!cache)
		return false;

	capacity(*cache, cap);
	return true;
}

void
ircd::db::capacity(rocksdb::Cache &cache,
                   const size_t &cap)
{
	cache.SetCapacity(cap);
}

size_t
ircd::db::capacity(const rocksdb::Cache *const &cache)
{
	return cache? capacity(*cache): 0;
}

size_t
ircd::db::capacity(const rocksdb::Cache &cache)
{
	return cache.GetCapacity();
}

///////////////////////////////////////////////////////////////////////////////
//
// Misc
//

std::vector<std::string>
ircd::db::column_names(const std::string &path,
                       const std::string &options)
{
	return column_names(path, database::options{options});
}

std::vector<std::string>
ircd::db::column_names(const std::string &path,
                       const rocksdb::DBOptions &opts)
try
{
	const ctx::uninterruptible ui;
	std::vector<std::string> ret;
	throw_on_error
	{
		rocksdb::DB::ListColumnFamilies(opts, path, &ret)
	};

	return ret;
}
catch(const io_error &e)
{
	return // No database found at path. Assume fresh.
	{
		{ rocksdb::kDefaultColumnFamilyName }
	};
}

ircd::db::database::options::options(const database &d)
:options{d.d->GetDBOptions()}
{
}

ircd::db::database::options::options(const database::column &c)
:options
{
	rocksdb::ColumnFamilyOptions
	{
		c.d->d->GetOptions(c.handle.get())
	}
}{}

ircd::db::database::options::options(const rocksdb::DBOptions &opts)
{
	throw_on_error
	{
		rocksdb::GetStringFromDBOptions(this, opts)
	};
}

ircd::db::database::options::options(const rocksdb::ColumnFamilyOptions &opts)
{
	throw_on_error
	{
		rocksdb::GetStringFromColumnFamilyOptions(this, opts)
	};
}

ircd::db::database::options::operator rocksdb::PlainTableOptions()
const
{
	rocksdb::PlainTableOptions ret;
	throw_on_error
	{
		rocksdb::GetPlainTableOptionsFromString(ret, *this, &ret)
	};

	return ret;
}

ircd::db::database::options::operator rocksdb::BlockBasedTableOptions()
const
{
	rocksdb::BlockBasedTableOptions ret;
	throw_on_error
	{
		rocksdb::GetBlockBasedTableOptionsFromString(ret, *this, &ret)
	};

	return ret;
}

ircd::db::database::options::operator rocksdb::ColumnFamilyOptions()
const
{
	rocksdb::ColumnFamilyOptions ret;
	throw_on_error
	{
		rocksdb::GetColumnFamilyOptionsFromString(ret, *this, &ret)
	};

	return ret;
}

ircd::db::database::options::operator rocksdb::DBOptions()
const
{
	rocksdb::DBOptions ret;
	throw_on_error
	{
		rocksdb::GetDBOptionsFromString(ret, *this, &ret)
	};

	return ret;
}

ircd::db::database::options::operator rocksdb::Options()
const
{
	rocksdb::Options ret;
	throw_on_error
	{
		rocksdb::GetOptionsFromString(ret, *this, &ret)
	};

	return ret;
}

ircd::db::database::options::map::map(const options &o)
{
	throw_on_error
	{
		rocksdb::StringToMap(o, this)
	};
}

ircd::db::database::options::map::operator rocksdb::PlainTableOptions()
const
{
	rocksdb::PlainTableOptions ret;
	throw_on_error
	{
		rocksdb::GetPlainTableOptionsFromMap(ret, *this, &ret)
	};

	return ret;
}

ircd::db::database::options::map::operator rocksdb::BlockBasedTableOptions()
const
{
	rocksdb::BlockBasedTableOptions ret;
	throw_on_error
	{
		rocksdb::GetBlockBasedTableOptionsFromMap(ret, *this, &ret)
	};

	return ret;
}

ircd::db::database::options::map::operator rocksdb::ColumnFamilyOptions()
const
{
	rocksdb::ColumnFamilyOptions ret;
	throw_on_error
	{
		rocksdb::GetColumnFamilyOptionsFromMap(ret, *this, &ret)
	};

	return ret;
}

ircd::db::database::options::map::operator rocksdb::DBOptions()
const
{
	rocksdb::DBOptions ret;
	throw_on_error
	{
		rocksdb::GetDBOptionsFromMap(ret, *this, &ret)
	};

	return ret;
}

///////////////////////////////////////////////////////////////////////////////
//
// Misc
//

rocksdb::DBOptions
ircd::db::make_dbopts(std::string optstr,
                      std::string *const &out,
                      bool *const read_only,
                      bool *const fsck)
{
	// RocksDB doesn't parse a read_only option, so we allow that to be added
	// to open the database as read_only and then remove that from the string.
	if(read_only)
		*read_only = optstr_find_and_remove(optstr, "read_only=true;"s);
	else
		optstr_find_and_remove(optstr, "read_only=true;"s);

	// We also allow the user to specify fsck=true to run a repair operation on
	// the db. This may be expensive to do by default every startup.
	if(fsck)
		*fsck = optstr_find_and_remove(optstr, "fsck=true;"s);
	else
		optstr_find_and_remove(optstr, "fsck=true;"s);

	// Generate RocksDB options from string
	rocksdb::DBOptions opts
	{
		database::options(optstr)
	};

	if(out)
		*out = std::move(optstr);

	return opts;
}

bool
ircd::db::optstr_find_and_remove(std::string &optstr,
                                 const std::string &what)
{
	const auto pos(optstr.find(what));
	if(pos == std::string::npos)
		return false;

	optstr.erase(pos, what.size());
	return true;
}

rocksdb::ReadOptions
ircd::db::make_opts(const gopts &opts)
{
	rocksdb::ReadOptions ret;
	assert(ret.fill_cache);
	ret.read_tier = BLOCKING;

	// slice* for exclusive upper bound. when prefixes are used this value must
	// have the same prefix because ordering is not guaranteed between prefixes
	ret.iterate_lower_bound = opts.lower_bound;
	ret.iterate_upper_bound = opts.upper_bound;

	ret += opts;
	return ret;
}

ircd::conf::item<bool>
read_checksum
{
	{ "name",     "ircd.db.read.checksum" },
	{ "default",  false                   }
};

rocksdb::ReadOptions &
ircd::db::operator+=(rocksdb::ReadOptions &ret,
                     const gopts &opts)
{
	ret.iter_start_seqnum = opts.seqnum;
	ret.readahead_size = opts.readahead;

	if(opts.snapshot && !test(opts, get::NO_SNAPSHOT))
		ret.snapshot = opts.snapshot;

	ret.pin_data = test(opts, get::PIN);
	ret.fill_cache |= test(opts, get::CACHE);
	ret.fill_cache &= !test(opts, get::NO_CACHE);
	ret.tailing = test(opts, get::NO_SNAPSHOT);
	ret.prefix_same_as_start = test(opts, get::PREFIX);
	ret.total_order_seek = test(opts, get::ORDERED);
	ret.verify_checksums = bool(read_checksum);
	ret.verify_checksums |= test(opts, get::CHECKSUM);
	ret.verify_checksums &= !test(opts, get::NO_CHECKSUM);
	return ret;
}

rocksdb::WriteOptions
ircd::db::make_opts(const sopts &opts)
{
	rocksdb::WriteOptions ret;
	//ret.no_slowdown = true;    // read_tier = NON_BLOCKING for writes
	ret += opts;
	return ret;
}

rocksdb::WriteOptions &
ircd::db::operator+=(rocksdb::WriteOptions &ret,
                     const sopts &opts)
{
	ret.sync = test(opts, set::FSYNC);
	ret.disableWAL = test(opts, set::NO_JOURNAL);
	ret.ignore_missing_column_families = test(opts, set::MISSING_COLUMNS);
	return ret;
}

void
ircd::db::valid_eq_or_throw(const rocksdb::Iterator &it,
                            const string_view &sv)
{
	assert(!empty(sv));
	if(!valid_eq(it, sv))
	{
		throw_on_error(it.status());
		throw not_found{};
	}
}

void
ircd::db::valid_or_throw(const rocksdb::Iterator &it)
{
	if(!valid(it))
	{
		throw_on_error(it.status());
		throw not_found{};
		//assert(0); // status == ok + !Valid() == ???
	}
}

bool
ircd::db::valid_lte(const rocksdb::Iterator &it,
                    const string_view &sv)
{
	return valid(it, [&sv](const auto &it)
	{
		return it.key().compare(slice(sv)) <= 0;
	});
}

bool
ircd::db::valid_gt(const rocksdb::Iterator &it,
                   const string_view &sv)
{
	return valid(it, [&sv](const auto &it)
	{
		return it.key().compare(slice(sv)) > 0;
	});
}

bool
ircd::db::valid_eq(const rocksdb::Iterator &it,
                   const string_view &sv)
{
	return valid(it, [&sv](const auto &it)
	{
		return it.key().compare(slice(sv)) == 0;
	});
}

bool
ircd::db::valid(const rocksdb::Iterator &it,
                const valid_proffer &proffer)
{
	return valid(it)? proffer(it) : false;
}

bool
ircd::db::operator!(const rocksdb::Iterator &it)
{
	return !valid(it);
}

bool
ircd::db::valid(const rocksdb::Iterator &it)
{
	switch(it.status().code())
	{
		using rocksdb::Status;

		case Status::kOk:          break;
		case Status::kNotFound:    break;
		case Status::kIncomplete:  break;
		default:
			throw_on_error(it.status());
			__builtin_unreachable();
	}

	return it.Valid();
}

//
// error_to_status
//

ircd::db::error_to_status::error_to_status(const fs::error &e)
:error_to_status{e.code}
{
}

ircd::db::error_to_status::error_to_status(const std::exception &e)
:rocksdb::Status
{
	Status::Aborted(slice(string_view(e.what())))
}
{
}

ircd::db::error_to_status::error_to_status(const std::error_code &e)
:rocksdb::Status{[&e]
{
	using std::errc;

	switch(e.value())
	{
		case 0:
			return Status::OK();

		case int(errc::no_such_file_or_directory):
			return Status::NotFound();

		case int(errc::not_supported):
			return Status::NotSupported();

		case int(errc::invalid_argument):
			return Status::InvalidArgument();

		case int(errc::io_error):
			 return Status::IOError();

		case int(errc::timed_out):
			return Status::TimedOut();

		case int(errc::device_or_resource_busy):
			return Status::Busy();

		case int(errc::resource_unavailable_try_again):
			return Status::TryAgain();

		case int(errc::no_space_on_device):
			return Status::NoSpace();

		case int(errc::not_enough_memory):
			return Status::MemoryLimit();

		default:
			return Status::Aborted(slice(string_view(e.message())));
	}
}()}
{
}

//
// throw_on_error
//

ircd::db::throw_on_error::throw_on_error(const rocksdb::Status &s)
{
	using rocksdb::Status;

	switch(s.code())
	{
		case Status::kOk:                   return;
		case Status::kNotFound:             throw not_found("%s", s.ToString());
		case Status::kCorruption:           throw corruption("%s", s.ToString());
		case Status::kNotSupported:         throw not_supported("%s", s.ToString());
		case Status::kInvalidArgument:      throw invalid_argument("%s", s.ToString());
		case Status::kIOError:              throw io_error("%s", s.ToString());
		case Status::kMergeInProgress:      throw merge_in_progress("%s", s.ToString());
		case Status::kIncomplete:           throw incomplete("%s", s.ToString());
		case Status::kShutdownInProgress:   throw shutdown_in_progress("%s", s.ToString());
		case Status::kTimedOut:             throw timed_out("%s", s.ToString());
		case Status::kAborted:              throw aborted("%s", s.ToString());
		case Status::kBusy:                 throw busy("%s", s.ToString());
		case Status::kExpired:              throw expired("%s", s.ToString());
		case Status::kTryAgain:             throw try_again("%s", s.ToString());
		default: throw error
		{
			"code[%d] %s", s.code(), s.ToString()
		};
	}
}

//
//
//

std::vector<std::string>
ircd::db::available()
{
	const auto prefix
	{
		fs::get(fs::DB)
	};

	const auto dirs
	{
		fs::ls(prefix)
	};

	std::vector<std::string> ret;
	for(const auto &dir : dirs)
	{
		const auto name
		{
			lstrip(dir, prefix)
		};

		const auto checkpoints
		{
			fs::ls(dir)
		};

		for(const auto cpdir : checkpoints) try
		{
			const auto checkpoint
			{
				lstrip(lstrip(cpdir, dir), '/') //TODO: x-platform
			};

			auto path
			{
				db::path(name, lex_cast<uint64_t>(checkpoint))
			};

			ret.emplace_back(std::move(path));
		}
		catch(const bad_lex_cast &e)
		{
			continue;
		}
	}

	return ret;
}

std::string
ircd::db::path(const string_view &name)
{
	const auto pair
	{
		namepoint(name)
	};

	return path(pair.first, pair.second);
}

std::string
ircd::db::path(const string_view &name,
               const uint64_t &checkpoint)
{
	const auto prefix
	{
		fs::get(fs::DB)
	};

	return fs::make_path(
	{
		prefix, name, lex_cast(checkpoint)
	});
}

std::pair<ircd::string_view, uint64_t>
ircd::db::namepoint(const string_view &name_)
{
	const auto s
	{
		split(name_, ':')
	};

	return
	{
		s.first,
		s.second? lex_cast<uint64_t>(s.second) : uint64_t(-1)
	};
}

std::string
ircd::db::namepoint(const string_view &name,
                    const uint64_t &checkpoint)
{
	return std::string{name} + ':' + std::string{lex_cast(checkpoint)};
}

std::pair<ircd::string_view, ircd::string_view>
ircd::db::operator*(const rocksdb::Iterator &it)
{
	return { key(it), val(it) };
}

ircd::string_view
ircd::db::key(const rocksdb::Iterator &it)
{
	return slice(it.key());
}

ircd::string_view
ircd::db::val(const rocksdb::Iterator &it)
{
	return slice(it.value());
}

const char *
ircd::db::data(const rocksdb::Slice &slice)
{
	return slice.data();
}

size_t
ircd::db::size(const rocksdb::Slice &slice)
{
	return slice.size();
}

rocksdb::Slice
ircd::db::slice(const string_view &sv)
{
	return { sv.data(), sv.size() };
}

ircd::string_view
ircd::db::slice(const rocksdb::Slice &sk)
{
	return { sk.data(), sk.size() };
}

const std::string &
ircd::db::reflect(const rocksdb::Tickers &type)
{
	const auto &names(rocksdb::TickersNameMap);
	const auto it(std::find_if(begin(names), end(names), [&type]
	(const auto &pair)
	{
		return pair.first == type;
	}));

	static const auto empty{"<ticker>?????"s};
	return it != end(names)? it->second : empty;
}

const std::string &
ircd::db::reflect(const rocksdb::Histograms &type)
{
	const auto &names(rocksdb::HistogramsNameMap);
	const auto it(std::find_if(begin(names), end(names), [&type]
	(const auto &pair)
	{
		return pair.first == type;
	}));

	static const auto empty{"<histogram>?????"s};
	return it != end(names)? it->second : empty;
}

ircd::string_view
ircd::db::reflect(const pos &pos)
{
	switch(pos)
	{
		case pos::NEXT:     return "NEXT";
		case pos::PREV:     return "PREV";
		case pos::FRONT:    return "FRONT";
		case pos::BACK:     return "BACK";
		case pos::END:      return "END";
	}

	return "?????";
}

ircd::string_view
ircd::db::reflect(const op &op)
{
	switch(op)
	{
		case op::GET:             return "GET";
		case op::SET:             return "SET";
		case op::MERGE:           return "MERGE";
		case op::DELETE_RANGE:    return "DELETE_RANGE";
		case op::DELETE:          return "DELETE";
		case op::SINGLE_DELETE:   return "SINGLE_DELETE";
	}

	return "?????";
}

ircd::string_view
ircd::db::reflect(const rocksdb::Env::Priority &p)
{
	switch(p)
	{
		case rocksdb::Env::Priority::BOTTOM:  return "BOTTOM"_sv;
		case rocksdb::Env::Priority::LOW:     return "LOW"_sv;
		case rocksdb::Env::Priority::HIGH:    return "HIGH"_sv;
		case rocksdb::Env::Priority::TOTAL:   assert(0); break;
	}

	return "????"_sv;
}

ircd::string_view
ircd::db::reflect(const rocksdb::Env::IOPriority &p)
{
	switch(p)
	{
		case rocksdb::Env::IOPriority::IO_LOW:     return "IO_LOW"_sv;
		case rocksdb::Env::IOPriority::IO_HIGH:    return "IO_HIGH"_sv;
		case rocksdb::Env::IOPriority::IO_TOTAL:   assert(0); break;
	}

	return "IO_????"_sv;
}

ircd::string_view
ircd::db::reflect(const rocksdb::RandomAccessFile::AccessPattern &p)
{
	switch(p)
	{
		case rocksdb::RandomAccessFile::AccessPattern::NORMAL:      return "NORMAL"_sv;
		case rocksdb::RandomAccessFile::AccessPattern::RANDOM:      return "RANDOM"_sv;
		case rocksdb::RandomAccessFile::AccessPattern::SEQUENTIAL:  return "SEQUENTIAL"_sv;
		case rocksdb::RandomAccessFile::AccessPattern::WILLNEED:    return "WILLNEED"_sv;
		case rocksdb::RandomAccessFile::AccessPattern::DONTNEED:    return "DONTNEED"_sv;
	}

	return "??????"_sv;
}

bool
ircd::db::value_required(const op &op)
{
	switch(op)
	{
		case op::SET:
		case op::MERGE:
		case op::DELETE_RANGE:
			return true;

		case op::GET:
		case op::DELETE:
		case op::SINGLE_DELETE:
			return false;
	}

	assert(0);
	return false;
}
