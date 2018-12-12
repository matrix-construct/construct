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
#include <rocksdb/sst_dump_tool.h>
#include <rocksdb/compaction_filter.h>

// ircd::db interfaces requiring complete RocksDB (frontside).
#include <ircd/db/database/comparator.h>
#include <ircd/db/database/prefix_transform.h>
#include <ircd/db/database/compaction_filter.h>
#include <ircd/db/database/mergeop.h>
#include <ircd/db/database/events.h>
#include <ircd/db/database/stats.h>
#include <ircd/db/database/logger.h>
#include <ircd/db/database/column.h>
#include <ircd/db/database/txn.h>
#include <ircd/db/database/cache.h>

// RocksDB embedding environment callback interfaces (backside).
#include <ircd/db/database/env/env.h>
#include <ircd/db/database/env/writable_file.h>
#include <ircd/db/database/env/sequential_file.h>
#include <ircd/db/database/env/random_access_file.h>
#include <ircd/db/database/env/random_rw_file.h>
#include <ircd/db/database/env/directory.h>
#include <ircd/db/database/env/file_lock.h>
#include <ircd/db/database/env/state.h>

// Internal utility interface for this definition file.
#include "db.h"

// RocksDB port linktime-overriding interfaces (experimental).
#ifdef IRCD_DB_PORT
#include <ircd/db/database/env/port.h>
#endif

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

/// This mutex is necessary to serialize entry into rocksdb's write impl
/// otherwise there's a risk of a deadlock if their internal pthread
/// mutexes are contended. This is because a few parts of rocksdb are
/// incorrectly using std::mutex directly when they ought to be using their
/// rocksdb::port wrapper.
decltype(ircd::db::write_mutex)
ircd::db::write_mutex;

///////////////////////////////////////////////////////////////////////////////
//
// init
//

namespace ircd::db
{
	static std::string direct_io_test_file_path();
	static void init_test_direct_io();
	static void init_compressions();
	static void init_directory();
}

decltype(ircd::db::version)
ircd::db::version
{
	ROCKSDB_MAJOR,
	ROCKSDB_MINOR,
	ROCKSDB_PATCH
};

char ircd_db_version_str_buf[64];
decltype(ircd::db::version_str)
ircd::db::version_str
(
	ircd_db_version_str_buf,
	::snprintf(ircd_db_version_str_buf, sizeof(ircd_db_version_str_buf),
	           "%u.%u.%u",
	           version[0],
	           version[1],
	           version[2])
);

decltype(ircd::db::abi_version)
ircd::db::abi_version
{
	//TODO: Get lib version.
	0,
	0,
	0,
};

char ircd_db_abi_version_str_buf[64];
decltype(ircd::db::abi_version_str)
ircd::db::abi_version_str
(
	ircd_db_abi_version_str_buf,
	::snprintf(ircd_db_abi_version_str_buf, sizeof(ircd_db_abi_version_str_buf),
	           "%u.%u.%u",
	           abi_version[0],
	           abi_version[1],
	           abi_version[2])
);

//
// init::init
//

ircd::db::init::init()
{
	init_compressions();
	init_directory();
	init_test_direct_io();
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
ircd::db::init_test_direct_io()
try
{
	const auto test_file_path
	{
		direct_io_test_file_path()
	};

	if(fs::support::direct_io(test_file_path))
		log::debug
		{
			log, "Detected Direct-IO works by opening test file at `%s'",
			test_file_path
		};
	else
		log::warning
		{
			log, "Direct-IO is not supported in the database directory `%s'"
			"; Concurrent database queries will not be possible.",
			fs::get(fs::DB)
		};
}
catch(const std::exception &e)
{
	log::error
	{
		log, "Failed to test if Direct-IO possible with test file `%s'"
		"; Concurrent database queries will not be possible :%s",
		direct_io_test_file_path(),
		e.what()
	};
}

std::string
ircd::db::direct_io_test_file_path()
{
	const auto dbdir
	{
		fs::get(fs::DB)
	};

	const std::string parts[]
	{
		dbdir, "SUPPORTS_DIRECT_IO"s
	};

	return fs::make_path(parts);
}

decltype(ircd::db::compressions)
ircd::db::compressions;

void
ircd::db::init_compressions()
{
	auto supported
	{
		rocksdb::GetSupportedCompressions()
	};

	for(const rocksdb::CompressionType &type : supported)
	{
		auto &string(compressions.at(uint(type)));
		throw_on_error
		{
			rocksdb::GetStringFromCompressionType(&string, type)
		};
	}

	if(supported.empty())
		log::warning
		{
			"No compression libraries have been linked with the DB."
			" This is probably not what you want."
		};
}

///////////////////////////////////////////////////////////////////////////////
//
// database
//

/// Conf item toggles if full database checksum verification should occur
/// when any database is opened.
decltype(ircd::db::open_check)
ircd::db::open_check
{
	{ "name",     "ircd.db.open.check"  },
	{ "default",  false                 },
	{ "persist",  false                 },
};

/// Conf item determines the recovery mode to use when opening any database.
///
/// "absolute" - The default and is the same for an empty value. This means
/// any database corruptions are treated as an error on open and an exception
/// is thrown with nothing else done.
///
/// "point" - The database is rolled back to before any corruption. This will
/// lose some of the latest data last committed, but will open the database
/// and continue normally thereafter.
///
/// "skip" - The corrupted areas are skipped over and the database continues
/// normally with just those assets missing. This option is dangerous because
/// the database continues in a logically incoherent state which is only ok
/// for very specific applications.
///
/// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
///
/// IRCd's applications are NOT tolerant of a skip of recovery.
/// NEVER USE "skip" RECOVERY MODE.
///
/// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
///
decltype(ircd::db::open_recover)
ircd::db::open_recover
{
	{ "name",     "ircd.db.open.recover"  },
	{ "default",  "absolute"              },
	{ "persist",  false                   },
};

void
ircd::db::sync(database &d)
{
	const ctx::uninterruptible::nothrow ui;
	const std::lock_guard<decltype(write_mutex)> lock{write_mutex};
	log::debug
	{
		log, "'%s': @%lu SYNC WAL",
		name(d),
		sequence(d)
	};

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
	const ctx::uninterruptible::nothrow ui;
	const std::lock_guard<decltype(write_mutex)> lock{write_mutex};
	log::debug
	{
		log, "'%s': @%lu FLUSH WAL",
		name(d),
		sequence(d)
	};

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
ircd::db::compact(database &d,
                  const compactor &cb)
{
	static const std::pair<string_view, string_view> range
	{
		{}, {}
	};

	for(const auto &c : d.columns)
	{
		db::column column{*c};
		compact(column, range, -1, cb);
	}
}

void
ircd::db::compact(database &d,
                  const int &level,
                  const compactor &cb)
{
	for(const auto &c : d.columns)
	{
		db::column column{*c};
		compact(column, level, cb);
	}
}

void
ircd::db::check(database &d)
{
	assert(d.d);
	const ctx::uninterruptible::nothrow ui;
	throw_on_error
	{
		d.d->VerifyChecksum()
	};
}

void
ircd::db::resume(database &d)
{
	assert(d.d);
	const ctx::uninterruptible::nothrow ui;
	const std::lock_guard<decltype(write_mutex)> lock{write_mutex};
	const auto errors
	{
		db::errors(d)
	};

	log::debug
	{
		log, "'%s': Attempting to resume from %zu errors @%lu",
		name(d),
		errors.size(),
		sequence(d)
	};

	throw_on_error
	{
		d.d->Resume()
	};

	d.errors.clear();

	log::info
	{
		log, "'%s': Resumed normal operation at sequence number %lu; cleared %zu errors",
		name(d),
		sequence(d),
		errors.size()
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
	if(!d.checkpointer)
		throw error
		{
			"Checkpointing is not available for db(%p) '%s",
			&d,
			name(d)
		};

	const std::lock_guard<decltype(write_mutex)> lock{write_mutex};
	const ctx::uninterruptible::nothrow ui;
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
	const std::lock_guard<decltype(write_mutex)> lock{write_mutex};
	const ctx::uninterruptible::nothrow ui;

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
	const std::unordered_map<std::string, std::string> options
	{
		{ std::string{key}, std::string{val} }
	};

	const std::lock_guard<decltype(write_mutex)> lock{write_mutex};
	const ctx::uninterruptible::nothrow ui;
	throw_on_error
	{
		d.d->SetDBOptions(options)
	};
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

/// Get the list of WAL (Write Ahead Log) files.
std::vector<std::string>
ircd::db::wals(const database &cd)
{
	auto &d
	{
		const_cast<database &>(cd)
	};

	std::vector<std::unique_ptr<rocksdb::LogFile>> vec;
	throw_on_error
	{
		d.d->GetSortedWalFiles(vec)
	};

	std::vector<std::string> ret(vec.size());
	std::transform(begin(vec), end(vec), begin(ret), []
	(const auto &file)
	{
		return file->PathName();
	});

	return ret;
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
	std::vector<std::string> ret;
	auto &d(const_cast<database &>(cd));
	const ctx::uninterruptible::nothrow ui;
	throw_on_error
	{
		d.d->GetLiveFiles(ret, &msz, false)
	};

	return ret;
}

const std::vector<std::string> &
ircd::db::errors(const database &d)
{
	return d.errors;
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
	return d.row_cache.get();
}

const rocksdb::Cache *
ircd::db::cache(const database &d)
{
	return d.row_cache.get();
}

template<>
ircd::db::prop_int
ircd::db::property(const database &cd,
                   const string_view &name)
{
	uint64_t ret(0);
	database &d(const_cast<database &>(cd));
	const ctx::uninterruptible::nothrow ui;
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
	extern const description default_description;
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
,path
{
	db::path(this->name, this->checkpoint)
}
,optstr
{
	std::move(optstr)
}
,fsck
{
	false
}
,read_only
{
	false
}
,env
{
	std::make_shared<struct env>(this)
}
,stats
{
	std::make_shared<struct stats>(this)
}
,logger
{
	std::make_shared<struct logger>(this)
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

	//rocksdb::NewSstFileManager(env.get(), logger, {}, 0, true, nullptr, 0.05)
}
,row_cache
{
	std::make_shared<database::cache>(this, this->stats, 16_MiB)
}
,descriptors
{
	std::move(description)
}
,opts{[this]
{
	auto opts
	{
		std::make_unique<rocksdb::DBOptions>(make_dbopts(this->optstr, &this->optstr, &read_only, &fsck))
	};

	// Setup sundry
	opts->create_if_missing = true;
	opts->create_missing_column_families = true;

	// Uses thread_local counters in rocksdb and probably useless for ircd::ctx.
	opts->enable_thread_tracking = false;

	// MUST be 0 or std::threads are spawned in rocksdb.
	opts->max_file_opening_threads = 0;

	// TODO: We should hint rocksdb with a harder value so it doesn't
	// potentially eat up all our fd's.
	opts->max_open_files = -1; //ircd::info::rlimit_nofile / 4;

	// These values are known to not cause any internal rocksdb issues for us,
	// but perhaps making them more aggressive can be looked into.
	opts->max_background_compactions = 1;
	opts->max_background_flushes = 1;
	opts->max_background_jobs = 2;

	// MUST be 1 (no subcompactions) or rocksdb spawns internal std::thread.
	opts->max_subcompactions = 1;

	// Disable noise
	opts->stats_dump_period_sec = 0;

	// Disables the timer to delete unused files; this operation occurs
	// instead with our compaction operations so we don't need to complicate.
	opts->delete_obsolete_files_period_micros = 0;

	// These values prevent codepaths from being taken in rocksdb which may
	// introduce issues for ircd::ctx. We should still fully investigate
	// if any of these features can safely be used.
	opts->allow_concurrent_memtable_write = false;
	opts->enable_write_thread_adaptive_yield = false;
	opts->enable_pipelined_write = false;
	opts->write_thread_max_yield_usec = 0;
	opts->write_thread_slow_yield_usec = 0;

	// Detect if O_DIRECT is possible if db::init left a file in the
	// database directory claiming such. User can force no direct io
	// with program option at startup (i.e -nodirect).
	opts->use_direct_reads = bool(fs::fd::opts::direct_io_enable)?
		fs::exists(direct_io_test_file_path()):
		false;

	// Use the determined direct io value for writes as well.
	opts->use_direct_io_for_flush_and_compaction = opts->use_direct_reads;

	// Doesn't appear to be in effect when direct io is used. Not supported by
	// all filesystems so disabled for now.
	// TODO: use fs::support::fallocate() test similar to direct_io_test_file.
	opts->allow_fallocate = false;

	#ifdef RB_DEBUG
	opts->dump_malloc_stats = true;
	#endif

	// Default corruption tolerance is zero-tolerance; db fails to open with
	// error by default to inform the user. The rest of the options are
	// various relaxations for how to proceed.
	opts->wal_recovery_mode = rocksdb::WALRecoveryMode::kAbsoluteConsistency;

	// When corrupted after crash, the DB is rolled back before the first
	// corruption and erases everything after it, giving a consistent
	// state up at that point, though losing some recent data.
	if(string_view(open_recover) == "point")
		opts->wal_recovery_mode = rocksdb::WALRecoveryMode::kPointInTimeRecovery;

	// Skipping corrupted records will create gaps in the DB timeline where the
	// application (like a matrix timeline) cannot tolerate the unexpected gap.
	if(string_view(open_recover) == "skip")
		opts->wal_recovery_mode = rocksdb::WALRecoveryMode::kSkipAnyCorruptedRecords;

	// Tolerating corrupted records is very last-ditch for getting the database to
	// open in a catastrophe. We have no use for this option but should use it for
	//TODO: emergency salvage-mode.
	if(string_view(open_recover) == "tolerate")
		opts->wal_recovery_mode = rocksdb::WALRecoveryMode::kTolerateCorruptedTailRecords;

	// This prevents the creation of additional files when the DB first opens.
	// It should be set to false once a comprehensive compaction system is
	// implemented which can reap those files. Otherwise we'll run out of fd's.
	opts->avoid_flush_during_recovery = true;

	// Setup env
	opts->env = env.get();

	// Setup SST file mgmt
	opts->sst_file_manager = this->ssts;

	// Setup logging
	logger->SetInfoLogLevel(ircd::debugmode? rocksdb::DEBUG_LEVEL : rocksdb::WARN_LEVEL);
	opts->info_log_level = logger->GetInfoLogLevel();
	opts->info_log = logger;

	// Setup event and statistics callbacks
	opts->listeners.emplace_back(this->events);

	// Setup histogram collecting
	//this->stats->stats_level_ = rocksdb::kAll;
	this->stats->stats_level_ = rocksdb::kExceptTimeForMutex;
	opts->statistics = this->stats;

	// Setup performance metric options
	//rocksdb::SetPerfLevel(rocksdb::PerfLevel::kDisable);

	// Setup row cache.
	opts->row_cache = this->row_cache;

	return opts;
}()}
,column_names{[this]
{
	// Existing columns at path. If any are left the descriptor set did not
	// describe all of the columns found in the database at path.
	const auto required
	{
		db::column_names(path, *opts)
	};

	// As we find descriptors for all of the columns on the disk we'll
	// remove their names from this set. Anything remaining is undescribed
	// and that's a fatal error.
	std::set<string_view> existing
	{
		begin(required), end(required)
	};

	// The names of the columns extracted from the descriptor set
	decltype(this->column_names) ret;
	for(auto &descriptor : descriptors)
	{
		// Deprecated columns which have already been dropped won't appear
		// in the existing (required) list. We don't need to construct those.
		if(!existing.count(descriptor.name) && descriptor.drop)
			continue;

		// Construct the column instance and indicate that we have a description
		// for it by removing it from existing.
		ret.emplace(descriptor.name, std::make_shared<column>(*this, descriptor));
		existing.erase(descriptor.name);
	}

	for(const auto &remain : existing)
		throw error
		{
			"Failed to describe existing column '%s' (and %zd others...)",
			remain,
			existing.size() - 1
		};

	return ret;
}()}
,d{[this]
{
	std::vector<rocksdb::ColumnFamilyHandle *> handles; // filled by DB::Open()
	std::vector<rocksdb::ColumnFamilyDescriptor> columns(this->column_names.size());
	std::transform(begin(this->column_names), end(this->column_names), begin(columns), []
	(const auto &pair)
	{
		const auto &column(*pair.second);
		return static_cast<const rocksdb::ColumnFamilyDescriptor &>(column);
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
			rocksdb::RepairDB(path, *opts, columns)
		};

		log::info
		{
			log, "Database @ `%s' check complete", path
		};
	}

	// If the directory does not exist, though rocksdb will create it, we can
	// avoid scaring the user with an error log message if we just do that..
	if(opts->create_if_missing && !fs::is_dir(path))
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
			rocksdb::DB::OpenForReadOnly(*opts, path, columns, &handles, &ptr)
		};
	else
		throw_on_error
		{
			rocksdb::DB::Open(*opts, path, columns, &handles, &ptr)
		};

	std::unique_ptr<rocksdb::DB> ret
	{
		ptr
	};

	// Set the handles. We can't throw here so we just log an error.
	for(const auto &handle : handles) try
	{
		this->column_names.at(handle->GetName())->handle.reset(handle);
	}
	catch(const std::exception &e)
	{
		log::critical
		{
			"'%s': Error finding described handle '%s' which RocksDB opened :%s",
			this->name,
			handle->GetName(),
			e.what()
		};
	}

	return ret;
}()}
,column_index{[this]
{
	size_t size{0};
	for(const auto &p : column_names)
	{
		const auto &column(*p.second);
		if(db::id(column) + 1 > size)
			size = db::id(column) + 1;
	}

	// This may have some gaps containing nullptrs where a CFID is unused.
	decltype(this->column_index) ret(size);
	for(const auto &p : column_names)
	{
		const auto &colptr(p.second);
		ret.at(db::id(*colptr)) = colptr;
	}

	return ret;
}()}
,columns{[this]
{
	// Skip the gaps in the column_index vector to make the columns list
	// only contain active column instances.
	decltype(this->columns) ret;
	for(const auto &ptr : this->column_index)
		if(ptr)
			ret.emplace_back(ptr);

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
	// Conduct drops from schema changes. The database must be fully opened
	// as if they were not dropped first, then we conduct the drop operation
	// here. The drop operation has no effects until the database is next
	// closed; the dropped columns will still work during this instance.
	for(const auto &colptr : columns)
		if(describe(*colptr).drop)
			db::drop(*colptr);

	// Database integrity check branch.
	if(bool(open_check))
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
	log::debug
	{
		log, "'%s': flushed; closing columns...",
		name
	};

	this->checkpointer.reset(nullptr);
	this->column_names.clear();
	this->column_index.clear();
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
	const auto it{column_names.find(name)};
	if(unlikely(it == std::end(column_names)))
		throw schema_error
		{
			"'%s': column '%s' is not available or specified in schema",
			this->name,
			name
		};

	return operator[](db::id(*it->second));
}

ircd::db::database::column &
ircd::db::database::operator[](const uint32_t &id)
try
{
	auto &ret(*column_index.at(id));
	assert(db::id(ret) == id);
	return ret;
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
	const auto it{column_names.find(name)};
	if(unlikely(it == std::end(column_names)))
		throw schema_error
		{
			"'%s': column '%s' is not available or specified in schema",
			this->name,
			name
		};

	return operator[](db::id(*it->second));
}

const ircd::db::database::column &
ircd::db::database::operator[](const uint32_t &id)
const try
{
	auto &ret(*column_index.at(id));
	assert(db::id(ret) == id);
	return ret;
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

ircd::db::database::comparator::comparator(database *const &d,
                                           db::comparator user)
:d{d}
,user
{
	std::move(user)
}
{
}

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

bool
ircd::db::database::comparator::IsSameLengthImmediateSuccessor(const Slice &s,
                                                               const Slice &t)
const noexcept
{
	return rocksdb::Comparator::IsSameLengthImmediateSuccessor(s, t);
}

bool
ircd::db::database::comparator::CanKeysWithDifferentByteContentsBeEqual()
const noexcept
{
	// When keys with different byte contents can be equal the keys are
	// not hashable.
	return !user.hashable;
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
	if(!c.handle)
		return;

	database &d(c);
	const std::lock_guard<decltype(write_mutex)> lock{write_mutex};
	const ctx::uninterruptible::nothrow ui;
	log::debug
	{
		log, "'%s':'%s' @%lu DROPPING COLUMN",
		name(d),
		name(c),
		sequence(d)
	};

	throw_on_error
	{
		c.d->d->DropColumnFamily(c.handle.get())
	};

	log::notice
	{
		log, "'%s':'%s' @%lu DROPPED COLUMN",
		name(d),
		name(c),
		sequence(d)
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

const ircd::db::descriptor &
ircd::db::describe(const database::column &c)
{
	assert(c.descriptor);
	return *c.descriptor;
}

//
// database::column
//

ircd::db::database::column::column(database &d,
                                   db::descriptor &descriptor)
:rocksdb::ColumnFamilyDescriptor
(
	descriptor.name, database::options{descriptor.options}
)
,d{&d}
,descriptor{&descriptor}
,key_type{this->descriptor->type.first}
,mapped_type{this->descriptor->type.second}
,cmp{this->d, this->descriptor->cmp}
,prefix{this->d, this->descriptor->prefix}
,cfilter{this, this->descriptor->compactor}
,stats{std::make_shared<struct database::stats>(this->d)}
,handle
{
	nullptr, [&d](rocksdb::ColumnFamilyHandle *const handle)
	{
		assert(d.d);
		if(handle && d.d)
			d.d->DestroyColumnFamilyHandle(handle);
	}
}
{
	// If possible, deduce comparator based on type given in descriptor
	if(!this->descriptor->cmp.less)
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

	// Set the insert hint prefix extractor
	if(this->options.prefix_extractor)
		this->options.memtable_insert_with_hint_prefix_extractor = this->options.prefix_extractor;

	// Set the compaction filter
	this->options.compaction_filter = &this->cfilter;

	//this->options.paranoid_file_checks = true;

	// More stats reported by the rocksdb.stats property.
	this->options.report_bg_io_stats = true;

	// Set the compaction style; we don't override this in the descriptor yet.
	//this->options.compaction_style = rocksdb::kCompactionStyleNone;
	this->options.compaction_style = rocksdb::kCompactionStyleLevel;

	// Set the compaction priority; this should probably be in the descriptor
	// but this is currently selected for the general matrix workload.
	this->options.compaction_pri = rocksdb::CompactionPri::kOldestLargestSeqFirst;

	// Set filter reductions for this column. This means we expect a key to exist.
	this->options.optimize_filters_for_hits = this->descriptor->expect_queries_hit;

	// Compression type
	this->options.compression = find_supported_compression(this->descriptor->compression);
	//this->options.compression = rocksdb::kNoCompression;

	// Compression options
	this->options.compression_opts.enabled = true;
	this->options.compression_opts.max_dict_bytes = 0;//8_MiB;

	// Mimic the above for bottommost compression.
	//this->options.bottommost_compression = this->options.compression;
	//this->options.bottommost_compression_opts = this->options.compression_opts;

	//TODO: descriptor / conf
	this->options.num_levels = 7;
	//this->options.level0_file_num_compaction_trigger = 1;
	this->options.target_file_size_base = 32_MiB;
	//this->options.max_bytes_for_level_base = 192_MiB;
	this->options.target_file_size_multiplier = 2;        // size at level
	//this->options.max_bytes_for_level_multiplier = 3;        // size at level
	//this->options.write_buffer_size = 2_MiB;
	//this->options.disable_auto_compactions = true;
	//this->options.level_compaction_dynamic_level_bytes = true;

	//
	// Table options
	//

	// Block based table index type.
	table_opts.format_version = 3; // RocksDB >= 5.15 compat only; otherwise use 2.
	table_opts.index_type = rocksdb::BlockBasedTableOptions::kTwoLevelIndexSearch;
	table_opts.partition_filters = true;
	table_opts.use_delta_encoding = true;
	table_opts.enable_index_compression = false;
	table_opts.read_amp_bytes_per_bit = 8;

	// Specify that index blocks should use the cache. If not, they will be
	// pre-read into RAM by rocksdb internally. Because of the above
	// TwoLevelIndex + partition_filters configuration on RocksDB v5.15 it's
	// better to use pre-read except in the case of a massive database.
	table_opts.cache_index_and_filter_blocks = true;
	table_opts.cache_index_and_filter_blocks_with_high_priority = false;
	table_opts.pin_top_level_index_and_filter = false;
	table_opts.pin_l0_filter_and_index_blocks_in_cache = false;

	// Setup the block size
	table_opts.block_size = this->descriptor->block_size;
	table_opts.metadata_block_size = this->descriptor->meta_block_size;
	table_opts.block_size_deviation = 50;

	// Block alignment doesn't work if compression is enabled for this
	// column. If not, we want block alignment for direct IO.
	table_opts.block_align = this->options.compression == rocksdb::kNoCompression;

	// Setup the cache for assets.
	const auto &cache_size(this->descriptor->cache_size);
	if(cache_size != 0)
		table_opts.block_cache = std::make_shared<database::cache>(this->d, this->stats, cache_size);

	// Setup the cache for compressed assets.
	const auto &cache_size_comp(this->descriptor->cache_size_comp);
	if(cache_size_comp != 0)
		table_opts.block_cache_compressed = std::make_shared<database::cache>(this->d, this->stats, cache_size_comp);

	// Setup the bloom filter.
	const auto &bloom_bits(this->descriptor->bloom_bits);
	if(bloom_bits)
		table_opts.filter_policy.reset(rocksdb::NewBloomFilterPolicy(bloom_bits, false));

	// Tickers::READ_AMP_TOTAL_READ_BYTES / Tickers::READ_AMP_ESTIMATE_USEFUL_BYTES
	//table_opts.read_amp_bytes_per_bit = 8;

	// Finally set the table options in the column options.
	this->options.table_factory.reset(rocksdb::NewBlockBasedTableFactory(table_opts));

	log::debug
	{
		log, "schema '%s' column [%s => %s] cmp[%s] pfx[%s] lru:%s:%s bloom:%zu compression:%d %s",
		db::name(d),
		demangle(key_type.name()),
		demangle(mapped_type.name()),
		this->cmp.Name(),
		this->options.prefix_extractor? this->prefix.Name() : "none",
		cache_size? "YES": "NO",
		cache_size_comp? "YES": "NO",
		bloom_bits,
		int(this->options.compression),
		this->descriptor->name
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

ircd::db::database::column::operator
const rocksdb::ColumnFamilyOptions &()
const
{
	return options;
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
// database::logger
//

ircd::db::database::logger::logger(database *const &d)
:rocksdb::Logger{}
,d{d}
{
}

ircd::db::database::logger::~logger()
noexcept
{
}

rocksdb::Status
ircd::db::database::logger::Close()
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
ircd::db::database::logger::Logv(const char *const fmt,
                                 va_list ap)
noexcept
{
	Logv(rocksdb::InfoLogLevel::DEBUG_LEVEL, fmt, ap);
}

void
ircd::db::database::logger::LogHeader(const char *const fmt,
                                      va_list ap)
noexcept
{
	Logv(rocksdb::InfoLogLevel::DEBUG_LEVEL, fmt, ap);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsuggest-attribute=format"
void
ircd::db::database::logger::Logv(const rocksdb::InfoLogLevel level,
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

//
// ticker
//

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

//
// histogram
//

const struct ircd::db::histogram &
ircd::db::histogram(const database &d,
                    const string_view &key)
{
	return histogram(d, histogram_id(key));
}

const struct ircd::db::histogram &
ircd::db::histogram(const database &d,
                    const uint32_t &id)
{
	return d.stats->histogram.at(id);
}

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

//
// stats::stats
//

ircd::db::database::stats::stats(database *const &d)
:d{d}
{
}

ircd::db::database::stats::~stats()
noexcept
{
}

rocksdb::Status
ircd::db::database::stats::Reset()
noexcept
{
	ticker.fill(0);
	histogram.fill({0.0});
	return rocksdb::Status::OK();
}

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
	auto &data(histogram.at(type));

	data.time += time;
	data.hits++;

	data.max = std::max(data.max, double(time));
	data.avg = data.time / static_cast<long double>(data.hits);
}

void
ircd::db::database::stats::histogramData(const uint32_t type,
                                         rocksdb::HistogramData *const data)
const noexcept
{
	assert(data);
	const auto &h
	{
		histogram.at(type)
	};

	data->median = h.median;
	data->percentile95 = h.pct95;
	data->percentile99 = h.pct99;
	data->average = h.avg;
	data->standard_deviation = h.stddev;
	data->max = h.max;
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

//
// database::stats::passthru
//

ircd::db::database::stats::passthru::passthru(rocksdb::Statistics *const &a,
                                              rocksdb::Statistics *const &b)
:pass
{
	{ a, b }
}
{
}

ircd::db::database::stats::passthru::~passthru()
noexcept
{
}

[[noreturn]]
rocksdb::Status
ircd::db::database::stats::passthru::Reset()
noexcept
{
	throw assertive {"Unavailable for passthru"};
}

void
ircd::db::database::stats::passthru::recordTick(const uint32_t tickerType,
                                                const uint64_t count)
noexcept
{
	for(auto *const &pass : this->pass)
		pass->recordTick(tickerType, count);
}

void
ircd::db::database::stats::passthru::measureTime(const uint32_t histogramType,
                                                 const uint64_t time)
noexcept
{
	for(auto *const &pass : this->pass)
		pass->measureTime(histogramType, time);
}

bool
ircd::db::database::stats::passthru::HistEnabledForType(const uint32_t type)
const noexcept
{
	return std::all_of(begin(pass), end(pass), [&type]
	(const auto *const &pass)
	{
		return pass->HistEnabledForType(type);
	});
}

[[noreturn]]
uint64_t
ircd::db::database::stats::passthru::getTickerCount(const uint32_t tickerType)
const noexcept
{
	throw assertive {"Unavailable for passthru"};
}

[[noreturn]]
void
ircd::db::database::stats::passthru::setTickerCount(const uint32_t tickerType,
                                                    const uint64_t count)
noexcept
{
	throw assertive {"Unavailable for passthru"};
}

[[noreturn]]
void
ircd::db::database::stats::passthru::histogramData(const uint32_t type,
                                                   rocksdb::HistogramData *const data)
const noexcept
{
	throw assertive {"Unavailable for passthru"};
}

[[noreturn]]
uint64_t
ircd::db::database::stats::passthru::getAndResetTickerCount(const uint32_t tickerType)
noexcept
{
	throw assertive {"Unavailable for passthru"};
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
	log::info
	{
		rog, "'%s': flush complete: column[%s] path[%s] ctx[%lu] job[%d] writes[slow:%d stop:%d] seq[%zu -> %zu] reason:%d",
		d->name,
		info.cf_name,
		info.file_path,
		info.thread_id,
		info.job_id,
		info.triggered_writes_slowdown,
		info.triggered_writes_stop,
		info.smallest_seqno,
		info.largest_seqno,
		int(info.flush_reason)
	};
}

void
ircd::db::database::events::OnFlushBegin(rocksdb::DB *const db,
                                         const rocksdb::FlushJobInfo &info)
noexcept
{
	log::info
	{
		rog, "'%s': flush begin column[%s] ctx[%lu] job[%d] writes[slow:%d stop:%d] seq[%zu -> %zu] reason:%d",
		d->name,
		info.cf_name,
		info.thread_id,
		info.job_id,
		info.triggered_writes_slowdown,
		info.triggered_writes_stop,
		info.smallest_seqno,
		info.largest_seqno,
		int(info.flush_reason)
	};
}

void
ircd::db::database::events::OnCompactionCompleted(rocksdb::DB *const db,
                                                  const rocksdb::CompactionJobInfo &info)
noexcept
{
	log::info
	{
		rog, "'%s': compacted column[%s] ctx[%lu] job[%d] level[in:%d out:%d] files[in:%zu out:%zu] reason:%d :%s",
		d->name,
		info.cf_name,
		info.thread_id,
		info.job_id,
		info.base_input_level,
		info.output_level,
		info.input_files.size(),
		info.output_files.size(),
		int(info.compaction_reason),
		info.status.ToString()
	};
}

void
ircd::db::database::events::OnTableFileDeleted(const rocksdb::TableFileDeletionInfo &info)
noexcept
{
	log::debug
	{
		rog, "'%s': table file deleted: db[%s] path[%s] status[%d] job[%d]",
		d->name,
		info.db_name,
		info.file_path,
		int(info.status.code()),
		info.job_id
	};
}

void
ircd::db::database::events::OnTableFileCreated(const rocksdb::TableFileCreationInfo &info)
noexcept
{
	log::debug
	{
		rog, "'%s': table file created: db[%s] path[%s] status[%d] job[%d]",
		d->name,
		info.db_name,
		info.file_path,
		int(info.status.code()),
		info.job_id
	};
}

void
ircd::db::database::events::OnTableFileCreationStarted(const rocksdb::TableFileCreationBriefInfo &info)
noexcept
{
	log::debug
	{
		rog, "'%s': table file creating: db[%s] column[%s] path[%s] job[%d]",
		d->name,
		info.db_name,
		info.cf_name,
		info.file_path,
		info.job_id
	};
}

void
ircd::db::database::events::OnMemTableSealed(const rocksdb::MemTableInfo &info)
noexcept
{
	log::debug
	{
		rog, "'%s': memory table sealed: column[%s] entries[%lu] deletes[%lu]",
		d->name,
		info.cf_name,
		info.num_entries,
		info.num_deletes
	};
}

void
ircd::db::database::events::OnColumnFamilyHandleDeletionStarted(rocksdb::ColumnFamilyHandle *const h)
noexcept
{
	log::debug
	{
		rog, "'%s': column[%s] handle closing @ %p",
		d->name,
		h->GetName(),
		h
	};
}

void
ircd::db::database::events::OnExternalFileIngested(rocksdb::DB *const d,
                                                   const rocksdb::ExternalFileIngestionInfo &info)
noexcept
{
	log::notice
	{
		rog, "'%s': external file ingested column[%s] external[%s] internal[%s] sequence:%lu",
		this->d->name,
		info.cf_name,
		info.external_file_path,
		info.internal_file_path,
		info.global_seqno
	};
}

void
ircd::db::database::events::OnBackgroundError(rocksdb::BackgroundErrorReason reason,
                                              rocksdb::Status *const status)
noexcept
{
	assert(d);
	assert(status);

	thread_local char buf[1024];
	const string_view str{fmt::sprintf
	{
		buf, "%s error in %s :%s",
		reflect(status->severity()),
		reflect(reason),
		status->ToString()
	}};

	// This is a legitimate when we want to use it. If the error is not
	// suppressed the DB will enter read-only mode and will require a
	// call to db::resume() to clear the error (i.e by admin at console).
	const bool ignore
	{
		false
	};

	const log::facility fac
	{
		ignore?
			log::facility::DERROR:
			log::facility::ERROR
	};

	log::logf
	{
		log, fac, "'%s': %s", d->name, str
	};

	if(ignore)
	{
		*status = rocksdb::Status::OK();
		return;
	}

	// Downgrade select fatal errors to hard errors. If this downgrade
	// does not occur then it can never be cleared by a db::resume() and
	// the daemon must be restarted.

	if(reason == rocksdb::BackgroundErrorReason::kCompaction)
		if(status->severity() == rocksdb::Status::kFatalError)
			*status = rocksdb::Status(*status, rocksdb::Status::kHardError);

	// Save the error string to the database instance for later examination.
	d->errors.emplace_back(str);
}

void
ircd::db::database::events::OnStallConditionsChanged(const rocksdb::WriteStallInfo &info)
noexcept
{
	log::warning
	{
		rog, "'%s' stall condition column[%s] %s -> %s",
		d->name,
		info.cf_name,
		reflect(info.condition.prev),
		reflect(info.condition.cur)
	};
}

///////////////////////////////////////////////////////////////////////////////
//
// database::cache (internal)
//

decltype(ircd::db::database::cache::DEFAULT_SHARD_BITS)
ircd::db::database::cache::DEFAULT_SHARD_BITS
(
	std::min(std::log2(size_t(db::request_pool_size)), 8.0)
);

decltype(ircd::db::database::cache::DEFAULT_STRICT)
ircd::db::database::cache::DEFAULT_STRICT
{
	false
};

decltype(ircd::db::database::cache::DEFAULT_HI_PRIO)
ircd::db::database::cache::DEFAULT_HI_PRIO
{
	0.10
};

//
// cache::cache
//

ircd::db::database::cache::cache(database *const &d,
                                 std::shared_ptr<struct database::stats> stats,
                                 const ssize_t &initial_capacity)
:d{d}
,stats{std::move(stats)}
,c
{
	rocksdb::NewLRUCache
	(
		std::max(initial_capacity, ssize_t(0))
		,DEFAULT_SHARD_BITS
		,DEFAULT_STRICT
		,DEFAULT_HI_PRIO
	)
}
{
	assert(bool(c));
}

ircd::db::database::cache::~cache()
noexcept
{
}

const char *
ircd::db::database::cache::Name()
const noexcept
{
	assert(bool(c));
	return c->Name();
}

rocksdb::Status
ircd::db::database::cache::Insert(const Slice &key,
                                  void *const value,
                                  size_t charge,
                                  deleter del,
                                  Handle **const handle,
                                  Priority priority)
noexcept
{
	assert(bool(c));
	assert(bool(stats));

	const rocksdb::Status &ret
	{
		c->Insert(key, value, charge, del, handle, priority)
	};

	stats->recordTick(rocksdb::Tickers::BLOCK_CACHE_ADD, ret.ok());
	stats->recordTick(rocksdb::Tickers::BLOCK_CACHE_ADD_FAILURES, !ret.ok());
	stats->recordTick(rocksdb::Tickers::BLOCK_CACHE_DATA_BYTES_INSERT, ret.ok()? charge : 0UL);
	return ret;
}

rocksdb::Cache::Handle *
ircd::db::database::cache::Lookup(const Slice &key,
                                  Statistics *const statistics)
noexcept
{
	assert(bool(c));
	assert(bool(this->stats));

	database::stats::passthru passthru
	{
		this->stats.get(), statistics
	};

	rocksdb::Statistics *const s
	{
		statistics?
			dynamic_cast<rocksdb::Statistics *>(&passthru):
			dynamic_cast<rocksdb::Statistics *>(this->stats.get())
	};

	auto *const &ret
	{
		c->Lookup(key, s)
	};

	// Rocksdb's LRUCache stats are broke. The statistics ptr is null and
	// passing it to Lookup() does nothing internally. We have to do this
	// here ourselves :/

	this->stats->recordTick(rocksdb::Tickers::BLOCK_CACHE_HIT, bool(ret));
	this->stats->recordTick(rocksdb::Tickers::BLOCK_CACHE_MISS, !bool(ret));
	return ret;
}

bool
ircd::db::database::cache::Ref(Handle *const handle)
noexcept
{
	assert(bool(c));
	return c->Ref(handle);
}

bool
ircd::db::database::cache::Release(Handle *const handle,
                                   bool force_erase)
noexcept
{
	assert(bool(c));
	return c->Release(handle, force_erase);
}

void *
ircd::db::database::cache::Value(Handle *const handle)
noexcept
{
	assert(bool(c));
	return c->Value(handle);
}

void
ircd::db::database::cache::Erase(const Slice &key)
noexcept
{
	assert(bool(c));
	return c->Erase(key);
}

uint64_t
ircd::db::database::cache::NewId()
noexcept
{
	assert(bool(c));
	return c->NewId();
}

void
ircd::db::database::cache::SetCapacity(size_t capacity)
noexcept
{
	assert(bool(c));
	return c->SetCapacity(capacity);
}

void
ircd::db::database::cache::SetStrictCapacityLimit(bool strict_capacity_limit)
noexcept
{
	assert(bool(c));
	return c->SetStrictCapacityLimit(strict_capacity_limit);
}

bool
ircd::db::database::cache::HasStrictCapacityLimit()
const noexcept
{
	assert(bool(c));
	return c->HasStrictCapacityLimit();
}

size_t
ircd::db::database::cache::GetCapacity()
const noexcept
{
	assert(bool(c));
	return c->GetCapacity();
}

size_t
ircd::db::database::cache::GetUsage()
const noexcept
{
	assert(bool(c));
	return c->GetUsage();
}

size_t
ircd::db::database::cache::GetUsage(Handle *const handle)
const noexcept
{
	assert(bool(c));
	return c->GetUsage(handle);
}

size_t
ircd::db::database::cache::GetPinnedUsage()
const noexcept
{
	assert(bool(c));
	return c->GetPinnedUsage();
}

void
ircd::db::database::cache::DisownData()
noexcept
{
	assert(bool(c));
	return c->DisownData();
}

void
ircd::db::database::cache::ApplyToAllCacheEntries(callback cb,
                                                  bool thread_safe)
noexcept
{
	assert(bool(c));
	return c->ApplyToAllCacheEntries(cb, thread_safe);
}

void
ircd::db::database::cache::EraseUnRefEntries()
noexcept
{
	assert(bool(c));
	return c->EraseUnRefEntries();
}

std::string
ircd::db::database::cache::GetPrintableOptions()
const noexcept
{
	assert(bool(c));
	return c->GetPrintableOptions();
}

void
ircd::db::database::cache::TEST_mark_as_data_block(const Slice &key,
                                                   size_t charge)
noexcept
{
	assert(bool(c));
	return c->TEST_mark_as_data_block(key, charge);

}

///////////////////////////////////////////////////////////////////////////////
//
// database::compaction_filter
//

ircd::db::database::compaction_filter::compaction_filter(column *const &c,
                                                         db::compactor user)
:c{c}
,d{c->d}
,user{std::move(user)}
{
}

ircd::db::database::compaction_filter::~compaction_filter()
noexcept
{
}

rocksdb::CompactionFilter::Decision
ircd::db::database::compaction_filter::FilterV2(const int level,
                                                const Slice &key,
                                                const ValueType type,
                                                const Slice &oldval,
                                                std::string *const newval,
                                                std::string *const skip)
const noexcept
{
	const ctx::uninterruptible::nothrow ui;

	#ifdef RB_DEBUG_DB_ENV
	const auto typestr
	{
		type == kValue?
			"VALUE"_sv:
		type == kMergeOperand?
			"MERGE"_sv:
			"BLOB"_sv
	};

	log::debug
	{
		log, "'%s':'%s': compaction level:%d key:%zu@%p type:%s old:%zu@%p new:%p skip:%p",
		d->name,
		c->name,
		level,
		size(key),
		data(key),
		typestr,
		size(oldval),
		data(oldval),
		(const void *)newval,
		(const void *)skip
	};
	#endif

	const db::compactor::callback &callback
	{
		type == ValueType::kValue && user.value?
			user.value:

		type == ValueType::kMergeOperand && user.merge?
			user.merge:

		compactor::callback{}
	};

	if(!callback)
		return Decision::kKeep;

	const compactor::args args
	{
		level, slice(key), slice(oldval), newval, skip
	};

	switch(callback(args))
	{
		default:
		case db::op::GET:            return Decision::kKeep;
		case db::op::SET:            return Decision::kChangeValue;
		case db::op::DELETE:         return Decision::kRemove;
		case db::op::DELETE_RANGE:   return Decision::kRemoveAndSkipUntil;
	}
}

bool
ircd::db::database::compaction_filter::IgnoreSnapshots()
const noexcept
{
	return false;
}

const char *
ircd::db::database::compaction_filter::Name()
const noexcept
{
	assert(c);
	return db::name(*c).c_str();
}

///////////////////////////////////////////////////////////////////////////////
//
// database::sst
//

void
ircd::db::database::sst::tool(const vector_view<const string_view> &args)
{
	const ctx::uninterruptible::nothrow ui;

	static const size_t ARG_MAX {16};
	static const size_t ARG_MAX_LEN {256};
	thread_local char arg[ARG_MAX][ARG_MAX_LEN]
	{
		"./sst_dump"
	};

	size_t i(0);
	char *argv[ARG_MAX] { arg[i++] };
	for(; i < ARG_MAX - 1 && i - 1 < args.size(); ++i)
	{
		strlcpy(arg[i], args.at(i - 1));
		argv[i] = arg[i];
	}
	argv[i++] = nullptr;
	assert(i <= ARG_MAX);

	rocksdb::SSTDumpTool tool;
	const int ret
	{
		tool.Run(i, argv)
	};

	if(ret != 0)
		throw error
		{
			"Error from SST dump tool: return value: %d", ret
		};
}

//
// sst::dump::dump
//

ircd::db::database::sst::dump::dump(db::column column,
                                    const key_range &range,
                                    const string_view &path_)
{
	const ctx::uninterruptible::nothrow ui;
	database::column &c(column);
	const database &d(column);

	std::string path{path_};
	if(path.empty())
	{
		const string_view path_parts[]
		{
			fs::get(fs::DB), db::name(d), db::name(c)
		};

		path = fs::make_path(path_parts);
	}

	rocksdb::Options opts(d.d->GetOptions(c));
	rocksdb::EnvOptions eopts(opts);
	rocksdb::SstFileWriter writer
	{
		eopts, opts, c
	};

	throw_on_error
	{
		writer.Open(path)
	};

	size_t i(0);
	for(auto it(column.begin()); it != column.end(); ++it, ++i)
		throw_on_error
		{
			writer.Put(slice(it->first), slice(it->second))
		};

	rocksdb::ExternalSstFileInfo info;
	if(i)
		throw_on_error
		{
			writer.Finish(&info)
		};

	this->info.column = db::name(column);
	this->info.path = std::move(info.file_path);
	this->info.min_key = std::move(info.smallest_key);
	this->info.max_key = std::move(info.largest_key);
	this->info.min_seq = info.sequence_number;
	this->info.max_seq = info.sequence_number;
	this->info.size = info.file_size;
	this->info.entries = info.num_entries;
	this->info.version = info.version;
}

//
// sst::info::vector
//

ircd::db::database::sst::info::vector::vector(const database &d)
{
	this->reserve(db::file_count(d));
	for(const auto &c : d.columns)
	{
		db::column column{*c};
		for(auto &&info : vector(column))
			this->emplace_back(std::move(info));
	}
}

ircd::db::database::sst::info::vector::vector(const db::column &column)
{
	const ctx::uninterruptible::nothrow ui;
	database::column &c(const_cast<db::column &>(column));
	database &d(*c.d);

	rocksdb::ColumnFamilyMetaData cfmd;
	d.d->GetColumnFamilyMetaData(c, &cfmd);

	rocksdb::TablePropertiesCollection tpc;
	throw_on_error
	{
		d.d->GetPropertiesOfAllTables(c, &tpc)
	};

	size_t i(0);
	this->resize(std::max(cfmd.file_count, tpc.size()));
	for(rocksdb::LevelMetaData &level : cfmd.levels)
		for(rocksdb::SstFileMetaData md : level.files)
		{
			auto &info(this->at(i++));
			info.operator=(std::move(md));
			info.level = level.level;

			const auto path(info.path + info.name);
			auto tp(*tpc.at(path));
			info.operator=(std::move(tp));
			tpc.erase(path);
		}

	for(auto &&kv : tpc)
	{
		auto &info(this->at(i++));
		auto tp(*kv.second);
		info.operator=(std::move(tp));
		info.path = kv.first;
	}

	assert(i == this->size());
}

//
// sst::info::info
//

ircd::db::database::sst::info::info(const database &d_,
                                    const string_view &filename)
{
	auto &d(const_cast<database &>(d_));
	const ctx::uninterruptible::nothrow ui;

	std::vector<rocksdb::LiveFileMetaData> v;
	d.d->GetLiveFilesMetaData(&v);

	for(auto &md : v)
		if(md.name == filename)
		{
			rocksdb::TablePropertiesCollection tpc;
			throw_on_error
			{
				d.d->GetPropertiesOfAllTables(d[md.column_family_name], &tpc)
			};

			auto tp(*tpc.at(md.db_path + md.name));
			this->operator=(std::move(md));
			this->operator=(std::move(tp));
			return;
		}

	throw not_found
	{
		"No file named '%s' is live in database '%s'",
		filename,
		d.name
	};
}

ircd::db::database::sst::info &
ircd::db::database::sst::info::operator=(rocksdb::LiveFileMetaData &&md)
{
	name = std::move(md.name);
	path = std::move(md.db_path);
	column = std::move(md.column_family_name);
	size = std::move(md.size);
	min_seq = std::move(md.smallest_seqno);
	max_seq = std::move(md.largest_seqno);
	min_key = std::move(md.smallestkey);
	max_key = std::move(md.largestkey);
	num_reads = std::move(md.num_reads_sampled);
	level = std::move(md.level);
	compacting = std::move(md.being_compacted);
	return *this;
}

ircd::db::database::sst::info &
ircd::db::database::sst::info::operator=(rocksdb::SstFileMetaData &&md)
{
	name = std::move(md.name);
	path = std::move(md.db_path);
	size = std::move(md.size);
	min_seq = std::move(md.smallest_seqno);
	max_seq = std::move(md.largest_seqno);
	min_key = std::move(md.smallestkey);
	max_key = std::move(md.largestkey);
	num_reads = std::move(md.num_reads_sampled);
	compacting = std::move(md.being_compacted);
	return *this;
}

ircd::db::database::sst::info &
ircd::db::database::sst::info::operator=(rocksdb::TableProperties &&tp)
{
	column = std::move(tp.column_family_name);
	filter = std::move(tp.filter_policy_name);
	comparator = std::move(tp.comparator_name);
	merge_operator = std::move(tp.merge_operator_name);
	prefix_extractor = std::move(tp.prefix_extractor_name);
	compression = std::move(tp.compression_name);
	format = std::move(tp.format_version);
	cfid = std::move(tp.column_family_id);
	data_size = std::move(tp.data_size);
	index_size = std::move(tp.index_size);
	top_index_size = std::move(tp.top_level_index_size);
	filter_size = std::move(tp.filter_size);
	keys_size = std::move(tp.raw_key_size);
	values_size = std::move(tp.raw_value_size);
	index_parts = std::move(tp.index_partitions);
	data_blocks = std::move(tp.num_data_blocks);
	entries = std::move(tp.num_entries);
	range_deletes = std::move(tp.num_range_deletions);
	fixed_key_len = std::move(tp.fixed_key_len);
	created = std::move(tp.creation_time);
	oldest_key = std::move(tp.oldest_key_time);
	return *this;
}

///////////////////////////////////////////////////////////////////////////////
//
// database::wal
//

//
// wal::info::vector
//

ircd::db::database::wal::info::vector::vector(const database &d_)
{
	auto &d{const_cast<database &>(d_)};
	std::vector<std::unique_ptr<rocksdb::LogFile>> vec;
	throw_on_error
	{
		d.d->GetSortedWalFiles(vec)
	};

	this->resize(vec.size());
	for(size_t i(0); i < vec.size(); ++i)
		this->at(i).operator=(*vec.at(i));
}

//
// wal::info::info
//

ircd::db::database::wal::info::info(const database &d_,
                                    const string_view &filename)
{
	auto &d{const_cast<database &>(d_)};
	std::vector<std::unique_ptr<rocksdb::LogFile>> vec;
	throw_on_error
	{
		d.d->GetSortedWalFiles(vec)
	};

	for(const auto &ptr : vec)
		if(ptr->PathName() == filename)
		{
			this->operator=(*ptr);
			return;
		}

	throw not_found
	{
		"No file named '%s' is live in database '%s'",
		filename,
		d.name
	};
}

ircd::db::database::wal::info &
ircd::db::database::wal::info::operator=(const rocksdb::LogFile &lf)
{
	name = lf.PathName();
	number = lf.LogNumber();
	seq = lf.StartSequence();
	size = lf.SizeFileBytes();
	alive = lf.Type() == rocksdb::WalFileType::kAliveLogFile;

	return *this;
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

	if(options.use_direct_writes)
		*r = std::make_unique<writable_file_direct>(&d, name, options, true);
	else
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

	if(options.use_direct_writes)
		*r = std::make_unique<writable_file_direct>(&d, name, options, false);
	else
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
		std::ios::out |
		(trunc? std::ios::trunc : std::ios::openmode(0))
	};

	ret.direct = this->env_opts.use_direct_writes;
	ret.cloexec = this->env_opts.set_fd_cloexec;
	return ret;
}()}
,fd
{
	name, this->opts
}
,preallocation_block_size
{
	ircd::info::page_size
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

	// Workaround a RocksDB bug which doesn't propagate EnvOptions properly
	// on some constructions of WritableFile early on during db open. We'll
	// get an env_opts.allow_fallocate==true here while it should be false
	// from the DBOptions at d->opts. We use &= so it's not set to true when
	// the caller specifically wants it false just for them.
	assert(d && d->opts);
	this->env_opts.allow_fallocate &= d->opts->allow_fallocate;
	//assert(env_opts.allow_fallocate == d->opts->allow_fallocate);
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
	Close();

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
	std::unique_lock<decltype(mutex)> lock{mutex};

	if(!fd)
		return Status::OK();

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': wfile:%p fd:%d close",
		d.name,
		this,
		int(fd)
	};
	#endif

	fd = fs::fd{};
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

	fs::fsync_opts opts;
	fs::fdsync(fd, opts);
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

	fs::fsync_opts opts;
	fs::fdsync(fd, opts);
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

	fs::fsync_opts opts;
	fs::fsync(fd, opts);
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

	fs::write_opts wopts;
	wopts.priority = this->prio;
	fs::truncate(fd, size, wopts);
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

	if(opts.direct)
		return Status::OK();

	#if defined(HAVE_POSIX_FADVISE) && defined(FADV_DONTNEED)
	syscall(::posix_fadvise, fd, offset, length, FADV_DONTNEED);
	#endif

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

rocksdb::Status
ircd::db::database::env::writable_file::Append(const Slice &s)
noexcept try
{
	assert(!opts.direct);
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
		size(s),
	};
	#endif

	fs::write_opts wopts;
	wopts.priority = this->prio;
	const const_buffer buf
	{
		data(s), size(s)
	};

	fs::append(fd, buf, wopts);
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
	assert(!opts.direct);
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

	fs::write_opts wopts;
	wopts.priority = this->prio;
	wopts.offset = offset;
	const const_buffer buf
	{
		data(s), size(s)
	};

	fs::append(fd, buf, wopts);
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
ircd::db::database::env::writable_file::Allocate(uint64_t offset,
                                                 uint64_t length)
noexcept try
{
	const ctx::uninterruptible::nothrow ui;
	const std::lock_guard<decltype(mutex)> lock{mutex};

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': wfile:%p fd:%d allocate offset:%lu length:%lu%s%s",
		d.name,
		this,
		int(fd),
		offset,
		length,
		env_opts.fallocate_with_keep_size? " KEEP_SIZE" : "",
		env_opts.allow_fallocate? "" : " (DISABLED)"
	};
	#endif

	if(!env_opts.allow_fallocate)
		return Status::NotSupported();

	_allocate(offset, length);
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

	if(!env_opts.allow_fallocate)
		return;

	_allocate(offset, length);
}

void
ircd::db::database::env::writable_file::_allocate(const size_t &offset,
                                                  const size_t &length)
{
	const size_t first_block
	{
		offset / preallocation_block_size
	};

	const size_t last_block
	{
		(offset + length) / preallocation_block_size
	};

	const ssize_t missing_blocks
	{
		ssize_t(last_block) - preallocation_last_block
	};

	// Fast bail when the offset and length are behind the last block already
	// allocated. We don't support windowing here. If this branch is not taken
	// we'll fallocate() contiguously from the last fallocate() (or offset 0).
	if(missing_blocks <= 0)
		return;

	const ssize_t start_block
	{
		preallocation_last_block + 1
	};

	const size_t allocate_offset
	{
		start_block * preallocation_block_size
	};

	const size_t allocate_length
	{
		missing_blocks * preallocation_block_size
	};

	fs::write_opts wopts;
	wopts.offset = allocate_offset;
	wopts.priority = this->prio;
	wopts.keep_size = env_opts.fallocate_with_keep_size;

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': wfile:%p fd:%d allocating %zd blocks after block:%zu offset:%lu length:%lu%s",
		d.name,
		this,
		int(fd),
		missing_blocks,
		start_block,
		allocate_offset,
		allocate_length,
		wopts.keep_size? " KEEP_SIZE" : ""
	};
	#endif

	assert(env_opts.allow_fallocate);
	assert(bool(d.opts));
	assert(d.opts->allow_fallocate);

	fs::allocate(fd, allocate_length, wopts);
	this->preallocation_last_block = last_block;
}

void
ircd::db::database::env::writable_file::GetPreallocationStatus(size_t *const block_size,
                                                               size_t *const last_allocated_block)
noexcept
{
	const ctx::uninterruptible::nothrow ui;
	const std::lock_guard<decltype(mutex)> lock{mutex};

	*block_size = this->preallocation_block_size;
	*last_allocated_block = this->preallocation_last_block;

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': wfile:%p get preallocation block_size(%p):%zu last_block(%p):%zu",
		d.name,
		this,
		block_size,
		*block_size,
		last_allocated_block,
		*last_allocated_block
	};
	#endif
}

void
ircd::db::database::env::writable_file::SetPreallocationBlockSize(size_t size)
noexcept
{
	const ctx::uninterruptible::nothrow ui;
	const std::lock_guard<decltype(mutex)> lock{mutex};

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

void
ircd::db::database::env::writable_file::SetIOPriority(Env::IOPriority prio)
noexcept
{
	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': wfile:%p IO priority %s",
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

void
ircd::db::database::env::writable_file::SetWriteLifeTimeHint(WriteLifeTimeHint hint)
noexcept
{
	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': wfile:%p hint %s",
		d.name,
		this,
		reflect(hint)
	};
	#endif

	this->hint = hint;
	//TODO: fcntl F_SET_FILE_RW_HINT
}

rocksdb::Env::WriteLifeTimeHint
ircd::db::database::env::writable_file::GetWriteLifeTimeHint()
noexcept
{
	return hint;
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

	//return size(fs::uuid(fd, buf));
	return 0;
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

//
// writable_file_direct
//

ircd::db::database::env::writable_file_direct::writable_file_direct(database *const &d,
                                                                    const std::string &name,
                                                                    const EnvOptions &env_opts,
                                                                    const bool &trunc)
:writable_file
{
	d, name, env_opts, trunc
}
,alignment
{
	fs::block_size(fd)
}
,logical_offset
{
	!trunc?
		fs::size(fd):
		size_t(0)
}
,buffer
{
	alignment, alignment
}
{
	zero(buffer);
	if(!aligned(logical_offset))
		throw assertive
		{
			"direct writable file requires read into buffer."
		};
}

rocksdb::Status
ircd::db::database::env::writable_file_direct::Close()
noexcept try
{
	const ctx::uninterruptible::nothrow ui;
	std::unique_lock<decltype(mutex)> lock{mutex};

	if(!fd)
		return Status::OK();

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': wfile:%p DIRECT fd:%d close",
		d.name,
		this,
		int(fd)
	};
	#endif

	if(logical_offset > 0 && fs::size(fd) != logical_offset)
	{
		fs::write_opts wopts;
		wopts.priority = this->prio;
		fs::truncate(fd, logical_offset, wopts);
	}

	fd = fs::fd{};
	return Status::OK();
}
catch(const fs::error &e)
{
	log::error
	{
		log, "'%s': wfile:%p DIRECT close :%s",
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
		log, "'%s': wfile:%p DIRECT close :%s",
		d.name,
		this,
		e.what()
	};

	return error_to_status{e};
}

rocksdb::Status
ircd::db::database::env::writable_file_direct::Truncate(uint64_t size)
noexcept try
{
	const ctx::uninterruptible::nothrow ui;
	const std::lock_guard<decltype(mutex)> lock{mutex};

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		"'%s': wfile:%p DIRECT fd:%d truncate to %lu bytes",
		d.name,
		this,
		int(fd),
		size
	};
	#endif

	fs::write_opts wopts;
	wopts.priority = this->prio;
	fs::truncate(fd, size, wopts);
	logical_offset = size;
	return Status::OK();
}
catch(const fs::error &e)
{
	log::error
	{
		log, "'%s': wfile:%p DIRECT fd:%d truncate to %lu bytes :%s",
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
		log, "'%s': wfile:%p DIRECT fd:%d truncate to %lu bytes :%s",
		d.name,
		this,
		int(fd),
		size,
		e.what()
	};

	return error_to_status{e};
}

rocksdb::Status
ircd::db::database::env::writable_file_direct::Append(const Slice &s)
noexcept try
{
	const ctx::uninterruptible::nothrow ui;
	const std::lock_guard<decltype(mutex)> lock{mutex};

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': wfile:%p DIRECT fd:%d append:%p%s bytes:%zu%s logical_offset:%zu%s",
		d.name,
		this,
		int(fd),
		data(s),
		aligned(data(s))? "" : "#AC",
		size(s),
		aligned(size(s))? "" : "#AC",
		logical_offset,
		aligned(logical_offset)? "" : "#AC"
	};
	#endif

	const auto logical_check
	{
		logical_offset
	};

	const_buffer buf
	{
		slice(s)
	};

	while(!empty(buf))
		buf = write(buf);

	assert(logical_check + size(slice(s)) == logical_offset);
	return Status::OK();
}
catch(const fs::error &e)
{
	log::error
	{
		log, "'%s': wfile:%p DIRECT fd:%d append:%p size:%zu :%s",
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
		log, "'%s': wfile:%p DIRECT fd:%d append:%p size:%zu :%s",
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
ircd::db::database::env::writable_file_direct::PositionedAppend(const Slice &s,
                                                                uint64_t offset)
noexcept
{
	const ctx::uninterruptible::nothrow ui;
	const std::lock_guard<decltype(mutex)> lock{mutex};

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': wfile:%p DIRECT fd:%d append:%p%s bytes:%zu%s offset:%zu%s",
		d.name,
		this,
		int(fd),
		data(s),
		aligned(data(s))? "" : "#AC",
		size(s),
		aligned(size(s))? "" : "#AC",
		offset,
		aligned(offset)? "" : "#AC"
	};
	#endif

	return rocksdb::Status::NotSupported();
}

uint64_t
ircd::db::database::env::writable_file_direct::GetFileSize()
noexcept try
{
	const ctx::uninterruptible::nothrow ui;
	const std::lock_guard<decltype(mutex)> lock{mutex};

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': wfile:%p DIRECT fd:%d get file size",
		d.name,
		this,
		int(fd)
	};
	#endif

	const auto &ret
	{
		logical_offset
	};

	assert(ret <= fs::size(fd));
	return ret;
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "'%s': wfile:%p DIRECT fd:%d get file size :%s",
		d.name,
		this,
		int(fd),
		e.what()
	};

	return 0;
}

/// (Internal) Append buffer. This function is the internal entry interface
/// for appending a buffer of any size and alignment to the file. It is
/// internal because it does no locking or error handling back to rocksdb,
/// because it's expected to be called from some virtual override which does
/// those things. This function will branch off as required to other internal
/// write_* functions to properly align and rebuffer the supplied buffer
/// eventually culminating in an aligned append to the file.
///
/// Calling this function will always result in some write to the file; even
/// if temporary buffering is used to achieve alignment; even if the entire
/// supplied buffer is hopelessly unaligned: the supplied data will be written
/// out some way or another during this call. This means there is no
/// requirement to care about flushing the temporary this->buffer after this
/// call is made. Note that the temporary this->buffer has no reason to be
/// touched by anything other than this function stack.
///
/// !!! NOTE !!!
/// There is a requirement to truncate the file after this call is made before
/// closing the file. If a crash occurs after a write() which was padded out
/// to the block alignment: the file size will reflect the padding when it is
/// opened at next startup; RocksDB will not detect its terminator character
/// sequence and consider this file corrupt.
/// !!!
///
/// - any offset
/// - any data
/// - any size
ircd::const_buffer
ircd::db::database::env::writable_file_direct::write(const const_buffer &buf_)
{
	const_buffer buf
	{
		// If the file's offset is aligned and the buffer's data is aligned
		// we take an easy branch which writes everything and copies any
		// unaligned overflow to the temporary this->buffer. Nothing is
		// returned into buf from this branch so there's nothing else done
		// as this function will return when empty(buf) is checked below.
		aligned(logical_offset) && aligned(data(buf_))?
			write_aligned(buf_):

		// If the file's offset isn't aligned we have to bring it up to
		// alignment first by using data from the front of buf_. All the
		// remaining data will be returned to here, which may make a mess
		// of buf's alignment and size but this frame will deal with that.
		!aligned(logical_offset)?
			write_unaligned_off(buf_):

		// The file's offset is aligned but buf is not aligned. We'll deal
		// with that in this frame.
			buf_
	};

	assert(aligned(logical_offset) || empty(buf));

	// buf can be empty here if it was entirely dealt with by the above
	// branches and there's nothing else to do here.
	if(empty(buf))
		return buf;

	// Branch on whether the buffer's address is aligned. If so, considering
	// the logical_offset is aligned here we are then finished.
	if(aligned(data(buf)))
		return write_aligned(buf);

	// Deal with an unaligned buffer by bringing it up to alignment. This
	// will end up returning an aligned buffer, but may unalign the
	// logical_offset by doing so. This write() call must be looped until
	// it empties the buffer. It will be loopy if everything comes very
	// unaligned out of rocksdb.
	return write_unaligned_buf(buf);
}

/// Called when the logical_offset aligned but the supplied buffer's address
/// is not aligned. The supplied buffer's size can be unaligned here. This
/// function will fill up the temporary this->buffer with the front of buf
/// until an aligned address is achieved.
///
/// The rest of the buffer which starts at an aligned address is returned and
/// not written. It is not written since this function may leave the
/// logical_offset at an unaligned address.
///
/// * aligned offset
/// * unaligned data
/// - any size
ircd::const_buffer
ircd::db::database::env::writable_file_direct::write_unaligned_buf(const const_buffer &buf)
{
	assert(aligned(logical_offset));
	assert(!aligned(data(buf)));
	assert(!aligned(buf));

	// Window on the data between the given buffer's pointer and the next
	// alignment boundary.
	const const_buffer under_buf
	{
		data(buf), std::min(remain(uintptr_t(data(buf))), size(buf))
	};

	// Window on the data from the alignment boundary to the end of the
	// given buffer.
	const const_buffer remaining_buf
	{
		buf + size(under_buf)
	};

	assert(size(under_buf) <= size(buf));
	assert(size(under_buf) + size(remaining_buf) == size(buf));
	assert(data(buf) + size(under_buf) == data(remaining_buf));
	assert(aligned(data(remaining_buf)) || empty(remaining_buf));

	// We have to use the temporary buffer to deal with the unaligned
	// leading part of the buffer. Since logical_offset is aligned this
	// buffer isn't being used right now. We copy as much as possible
	// to fill out a complete block, both the unaligned and aligned inputs
	// and zero padding if both are not sufficient.
	mutable_buffer dst(this->buffer);
	consume(dst, copy(dst, under_buf));
	consume(dst, copy(dst, remaining_buf));
	consume(dst, zero(dst));
	assert(empty(dst));

	// Flush the temporary buffer.
	_write__aligned(this->buffer, logical_offset);

	// The logical_offset is only advanced by the underflow amount, even if
	// we padded the temporary buffer with some remaing_buf data. The caller
	// is lead to believe they must deal with remaining_buf in its entirety
	// starting at the logical_offset.
	logical_offset += size(under_buf);

	return remaining_buf;
}

/// Called when the logical_offset is not aligned, indicating that something
/// was left in the temporary this->buffer which must be completed out to
/// alignment by consuming the front of the argument buf. This function appends
/// the front of buf to this->buffer and flushes this->buffer.
///
/// logical_offset is incremented, either to the next block alignment or less
/// if size(buf) can't get it there.
///
/// The rest of buf which isn't used to fill out this->buffer is returned and
/// not written. It is not written since the returned data(buf) might not
/// be aligned. In fact, this function does not care about the alignment of buf
/// at all.
///
/// * unaligned offset
/// - any data
/// - any size
ircd::const_buffer
ircd::db::database::env::writable_file_direct::write_unaligned_off(const const_buffer &buf)
{
	assert(!aligned(logical_offset));

	// Window on the amount of buf we can take to fill up remaining space in
	// the temporary this->buffer
	const const_buffer src
	{
		data(buf), std::min(size(buf), buffer_remain())
	};

	// Window on the remaining space in the temporary this->buffer.
	const mutable_buffer dst
	{
		this->buffer + buffer_consumed()
	};

	// Window on the remaining space in dst after src is copied to dst, if any.
	const mutable_buffer pad
	{
		dst + size(src)
	};

	assert(size(dst) - size(pad) == size(src));
	assert(size(src) + size(pad) == buffer_remain());
	assert(size(src) + size(pad) + buffer_consumed() == alignment);
	assert(size(src) + buffer_consumed() != alignment || empty(pad));

	copy(dst, src);
	zero(pad);

	// Backtrack the logical_offset to the aligned offset where this->buffer's
	// data starts.
	const auto aligned_offset
	{
		align(logical_offset)
	};

	// Write the whole temporary this->buffer at the aligned offset.
	_write__aligned(this->buffer, aligned_offset);

	// Only increment the logical_offset to indicate the appending of
	// what this function added to the temporary this->buffer.
	logical_offset += size(src);

	// The logical_offset should either be aligned now after using buf's
	// data to eliminate the temporary this->buffer, or buf's data wasn't
	// enough and we'll have to call this function again later with more.
	assert(aligned(logical_offset) || size(buf) < alignment);

	// Return the rest of buf which we didn't use to fill out this->buf
	// Caller will have to deal figuring out how to align the next write.
	return const_buffer
	{
		buf + size(src)
	};
}

/// Write function callable when the current logical_offset and the supplied
/// buffer's pointer are both aligned, but the size of the buffer need not
/// be aligned. This function thus assumes that the temporary this->buffer
/// is empty; it will write as much of the input buffer as aligned. The
/// unaligned overflow will be copied to the front of the temporary
/// this->buffer which will be padded to alignment and flushed and the
/// logical_offset will indicate an increment of the size of the input buffer.
///
/// * aligned offset
/// * aligned data
/// - any size
ircd::const_buffer
ircd::db::database::env::writable_file_direct::write_aligned(const const_buffer &buf)
{
	assert(aligned(data(buf)));
	assert(aligned(logical_offset));

	// This portion at the end of buf did not fill out to the alignment.
	const const_buffer overflow
	{
		_write_aligned(buf, logical_offset)
	};

	// The aligned portion was written so the offset is incremented here.
	logical_offset += size(buf) - size(overflow);

	assert(aligned(logical_offset));
	assert(size(overflow) < alignment);
	assert(aligned(data(overflow)) || empty(overflow));
	assert(align(size(buf)) + size(overflow) == size(buf));
	assert(blocks(size(buf)) * alignment + size(overflow) == size(buf));

	if(!empty(overflow))
	{
		// The overflow is copied to the temporary this->buffer, padded out with
		// zero and then flushed. The logical offset will be incremented by the
		// size of that overflow and will no longer be an aligned value,
		// indicating there is something in the temporary this->buffer.
		mutable_buffer dst(this->buffer);
		consume(dst, copy(dst, overflow));
		consume(dst, zero(dst));
		assert(empty(dst));

		_write__aligned(this->buffer, logical_offset);
		logical_offset += size(overflow);
		assert(!aligned(logical_offset));
	}

	// Nothing is ever returned and required by the caller here because the
	// input is aligned to its address and offset and any unaligned size was
	// dealt with using the temporary this->buffer.
	return {};
}

/// Lower level write to an aligned offset. The pointer of the buffer and the
/// offset both have to be aligned to alignment. The size of the buffer does
/// not have to be aligned to alignment. The unaligned portion of the input
/// buffer (the last partial block), if any, will be returned to the caller.
///
/// No modifications to the logical_offset or the temporary this->buffer take
/// place here so the caller must manipulate those accordingly.
///
/// * aligned data
/// * aligned offset
/// - any size
ircd::const_buffer
ircd::db::database::env::writable_file_direct::_write_aligned(const const_buffer &buf,
                                                              const uint64_t &offset)
{
	assert(aligned(data(buf)));
	assert(aligned(offset));

	// This portion will be written
	const const_buffer aligned_buf
	{
		data(buf), blocks(size(buf)) * alignment
	};

	// This trailing portion will be returned to caller
	const const_buffer ret
	{
		data(buf) + size(aligned_buf), size(buf) - size(aligned_buf)
	};

	assert(!empty(aligned_buf) || size(buf) < alignment);
	assert(size(aligned_buf) + size(ret) == size(buf));
	assert(size(ret) < alignment);

	// aligned_buf will be empty if buf itself is smaller than the alignment.
	if(empty(aligned_buf))
	{
		assert(size(ret) == size(buf));
		return ret;
	}

	_write__aligned(aligned_buf, offset);
	return ret;
}

/// Lowest level write of a fully aligned buffer to an aligned offset. The
/// pointer of the buffer, the size of the buffer, and the offset ALL have
/// to be aligned to alignment for this function. This function is the only
/// in the stack which actually writes to the filesystem.
///
/// No modifications to the logical_offset take place here so the caller must
/// increment that accordingly. The return value is a const_buffer to conform
/// with the rest of the stack but it is unconditionally empty here because
/// there is no possible overflowing.
///
/// * aligned offset
/// * aligned data
/// * aligned size
ircd::const_buffer
ircd::db::database::env::writable_file_direct::_write__aligned(const const_buffer &buf,
                                                               const uint64_t &offset)
{
	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': wfile:%p DIRECT fd:%d write:%p%s bytes:%zu%s offset:%zu%s (logical:%zu)",
		d.name,
		this,
		int(fd),
		data(buf),
		aligned(data(buf))? "" : "#AC",
		size(buf),
		aligned(size(buf))? "" : "#AC",
		offset,
		aligned(offset)? "" : "#AC",
		logical_offset
	};
	#endif

	assert(aligned(buf));
	assert(aligned(offset));

	fs::write_opts wopts;
	wopts.priority = this->prio;
	wopts.offset = offset;
	fs::write(fd, buf, wopts);

	// Nothing is ever returned to the caller here because the input buffer
	// and the offset must be fully aligned at this stage.
	return {};
}

size_t
ircd::db::database::env::writable_file_direct::buffer_consumed()
const
{
	return likely(alignment != 0)?
		logical_offset % alignment:
		0UL;
}

size_t
ircd::db::database::env::writable_file_direct::buffer_remain()
const
{
	return remain(logical_offset);
}

size_t
ircd::db::database::env::writable_file_direct::blocks(const size_t &value)
const
{
	return likely(alignment != 0)?
		value / alignment:
		0UL;
}

size_t
ircd::db::database::env::writable_file_direct::remain(const size_t &value)
const
{
	return likely(alignment != 0)?
		alignment - (value - align(value)):
		0UL;
}

size_t
ircd::db::database::env::writable_file_direct::align(const size_t &value)
const
{
	return likely(alignment != 0)?
		value - (value % alignment):
		value;
}

bool
ircd::db::database::env::writable_file_direct::aligned(const const_buffer &buf)
const
{
	return buffer::aligned(buf, alignment);
}

bool
ircd::db::database::env::writable_file_direct::aligned(const void *const &value)
const
{
	return aligned(size_t(value));
}

bool
ircd::db::database::env::writable_file_direct::aligned(const size_t &value)
const
{
	return (alignment == 0) || (value % alignment == 0UL);
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
,_buffer_align
{
	fs::block_size(fd)
}
,offset
{
	0
}
{
	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': opened seqfile:%p fd:%d bs:%zu '%s'",
		d->name,
		this,
		int(fd),
		_buffer_align,
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
	const std::unique_lock<decltype(mutex)> lock
	{
		mutex, std::try_to_lock
	};

	// RocksDB sez that this call requires "External synchronization" i.e the
	// caller, not this class is responsible for exclusion. We assert anyway.
	if(unlikely(!bool(lock)))
		throw assertive
		{
			"'%s': Unexpected concurrent access to seqfile %p",
			d.name,
			this
		};

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

	const const_buffer read
	{
		fs::read(fd, buf, offset)
	};

	*result = slice(read);
	this->offset += size(read);
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
	const std::unique_lock<decltype(mutex)> lock
	{
		mutex, std::try_to_lock
	};

	if(unlikely(!bool(lock)))
		throw assertive
		{
			"'%s': Unexpected concurrent access to seqfile %p",
			d.name,
			this
		};

	assert(result);
	assert(scratch);
	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': seqfile:%p offset:%zu positioned read:%p offset:%zu length:%zu scratch:%p",
		d.name,
		this,
		this->offset,
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

	const const_buffer read
	{
		fs::read(fd, buf, offset)
	};

	*result = slice(read);
	this->offset = std::max(this->offset, off_t(offset + size(read)));
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
	const std::unique_lock<decltype(mutex)> lock
	{
		mutex, std::try_to_lock
	};

	// RocksDB sez that this call requires "External synchronization" i.e the
	// caller, not this class is responsible for exclusion. We assert anyway.
	if(unlikely(!bool(lock)))
		throw assertive
		{
			"'%s': Unexpected concurrent access to seqfile %p",
			d.name,
			this
		};

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

	if(opts.direct)
		return Status::OK();

	#if defined(HAVE_POSIX_FADVISE) && defined(FADV_DONTNEED)
	syscall(::posix_fadvise, fd, offset, length, FADV_DONTNEED);
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
	const auto &ret
	{
		_buffer_align
	};

	return ret;
}

//
// random_access_file
//

decltype(ircd::db::database::env::random_access_file::default_opts)
ircd::db::database::env::random_access_file::default_opts{[]
{
	ircd::fs::fd::opts ret{std::ios_base::in};
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
,_buffer_align
{
	fs::block_size(fd)
}
{
	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': opened rfile:%p fd:%d bs:%zu '%s'",
		d->name,
		this,
		int(fd),
		_buffer_align,
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

	if(opts.direct)
		return Status::OK();

	#if defined(HAVE_POSIX_FADVISE) && defined(FADV_DONTNEED)
	syscall(::posix_fadvise, fd, offset, length, FADV_DONTNEED);
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

	//return size(fs::uuid(fd, buf));
	return 0;
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
	const auto &ret
	{
		_buffer_align
	};

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
,_buffer_align
{
	fs::block_size(fd)
}
{
	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "'%s': opened rwfile:%p fd:%d bs:%zu '%s'",
		d->name,
		this,
		int(fd),
		_buffer_align,
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
		log, "'%s': close rwfile:%p fd:%d '%s'",
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

	fs::fsync_opts opts;
	fs::fsync(fd, opts);
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

	fs::fsync_opts opts;
	fs::fdsync(fd, opts);
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

	fs::fsync_opts opts;
	fs::fdsync(fd, opts);
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
	const auto &ret
	{
		_buffer_align
	};

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
ircd::db::txn::handler::MarkBeginPrepare(bool b)
noexcept
{
	ircd::assertion("not implemented");
	return Status::OK();
}

rocksdb::Status
ircd::db::txn::handler::MarkEndPrepare(const Slice &xid)
noexcept
{
	ircd::assertion("not implemented");
	return Status::OK();
}

rocksdb::Status
ircd::db::txn::handler::MarkCommit(const Slice &xid)
noexcept
{
	ircd::assertion("not implemented");
	return Status::OK();
}

rocksdb::Status
ircd::db::txn::handler::MarkRollback(const Slice &xid)
noexcept
{
	ircd::assertion("not implemented");
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
	if(likely(!std::uncaught_exceptions()))
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
		//TODO: should check a bloom filter on the cache for this branch
		//TODO: because right now double-querying the cache is gross.
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
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstack-usage="
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
		std::transform(begin(d.column_names), end(d.column_names), colptr, [&colnames]
		(const auto &p)
		{
			return p.second.get();
		});
	else
		std::transform(begin(colnames), end(colnames), colptr, [&d]
		(const auto &name)
		{
			return &d[name];
		});

	std::vector<Iterator *> iterators;
	{
		// The goal here is to optimize away the heap allocation incurred by
		// having to pass RocksDB the specific std::vector type which doesn't
		// have room for an allocator. We use a single thread_local vector
		// and reserve() it with one worst-case size of all possible columns.
		// Then we resize it to this specific call's requirements and copy the
		// column pointers. On sane platforms only one allocation ever occurs.
		thread_local std::vector<ColumnFamilyHandle *> handles;
		assert(column_count <= d.columns.size());
		handles.reserve(d.columns.size());
		handles.resize(column_count);
		std::transform(colptr, colptr + column_count, begin(handles), []
		(database::column *const &ptr)
		{
			return ptr->handle.get();
		});

		// This has been seen to lead to IO and block the ircd::ctx;
		// specifically when background options are aggressive and shortly
		// after db opens.
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
#pragma GCC diagnostic pop

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
	uint64_t ret(0);
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

const ircd::db::descriptor &
ircd::db::describe(const column &column)
{
	const database::column &c(column);
	return describe(c);
}

std::vector<std::string>
ircd::db::files(const column &column)
{
	database::column &c(const_cast<db::column &>(column));
	database &d(*c.d);

	rocksdb::ColumnFamilyMetaData cfmd;
	d.d->GetColumnFamilyMetaData(c, &cfmd);

	size_t count(0);
	for(const auto &level : cfmd.levels)
		count += level.files.size();

	std::vector<std::string> ret;
	ret.reserve(count);
	for(auto &level : cfmd.levels)
		for(auto &file : level.files)
			ret.emplace_back(std::move(file.name));

	return ret;
}

void
ircd::db::drop(column &column)
{
	database::column &c(column);
	drop(c);
}

void
ircd::db::sort(column &column,
               const bool &blocking)
{
	database::column &c(column);
	database &d(*c.d);

	rocksdb::FlushOptions opts;
	opts.wait = blocking;

	const ctx::uninterruptible::nothrow ui;
	const std::lock_guard<decltype(write_mutex)> lock{write_mutex};
	log::debug
	{
		log, "'%s':'%s' @%lu FLUSH (sort) %s",
		name(d),
		name(c),
		sequence(d),
		blocking? "blocking"_sv: "non-blocking"_sv
	};

	throw_on_error
	{
		d.d->Flush(opts, c)
	};
}

void
ircd::db::compact(column &column,
                  const int &level_,
                  const compactor &cb)
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

		rocksdb::CompactionOptions opts;

		// RocksDB sez that setting this to Disable means that the column's
		// compression options are read instead. If we don't set this here,
		// rocksdb defaults to "snappy" (which is strange).
		opts.compression = rocksdb::kDisableCompressionOption;

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
		const ctx::uninterruptible ui;
		const std::lock_guard<decltype(write_mutex)> lock{write_mutex};

		// Save and restore the existing filter callback so we can allow our
		// caller to use theirs. Note that this manual compaction should be
		// exclusive for this column (no background compaction should be
		// occurring, at least one relying on this filter).
		auto their_filter(std::move(c.cfilter.user));
		const unwind unfilter{[&c, &their_filter]
		{
			c.cfilter.user = std::move(their_filter);
		}};

		c.cfilter.user = cb;

		log::debug
		{
			log, "'%s':'%s' COMPACT level:%d files:%zu size:%zu",
			name(d),
			name(c),
			level.level,
			level.files.size(),
			level.size
		};

		throw_on_error
		{
			d.d->CompactFiles(opts, c, files, level.level)
		};
	}
}

void
ircd::db::compact(column &column,
                  const std::pair<string_view, string_view> &range,
                  const int &to_level,
                  const compactor &cb)
{
	database &d(column);
	database::column &c(column);

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

	rocksdb::CompactRangeOptions opts;
	opts.change_level = to_level >= -1;
	opts.target_level = std::max(to_level, -1);
	opts.allow_write_stall = true;

	const ctx::uninterruptible ui;
	const std::lock_guard<decltype(write_mutex)> lock{write_mutex};

	// Save and restore the existing filter callback so we can allow our
	// caller to use theirs. Note that this manual compaction should be
	// exclusive for this column (no background compaction should be
	// occurring, at least one relying on this filter).
	auto their_filter(std::move(c.cfilter.user));
	const unwind unfilter{[&c, &their_filter]
	{
		c.cfilter.user = std::move(their_filter);
	}};

	c.cfilter.user = cb;

	log::debug
	{
		log, "'%s':'%s' @%lu COMPACT [%s, %s] to level %d",
		name(d),
		name(c),
		sequence(d),
		range.first,
		range.second,
		opts.target_level
	};

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
	database &d(column);
	database::column &c(column);
	const std::unordered_map<std::string, std::string> options
	{
		{ std::string{key}, std::string{val} }
	};

	const std::lock_guard<decltype(write_mutex)> lock{write_mutex};
	const ctx::uninterruptible::nothrow ui;
	throw_on_error
	{
		d.d->SetOptions(c, options)
	};
}

void
ircd::db::ingest(column &column,
                 const string_view &path)
{
	database &d(column);
	database::column &c(column);

	rocksdb::IngestExternalFileOptions opts;
	opts.allow_global_seqno = true;
	opts.allow_blocking_flush = true;

	// Automatically determine if we can avoid issuing new sequence
	// numbers by considering this ingestion as "backfill" of missing
	// data which did actually exist but was physically removed.
	const auto &copts{d.d->GetOptions(c)};
	opts.ingest_behind = copts.allow_ingest_behind;

	const std::vector<std::string> files
	{
		{ std::string{path} }
	};

	const std::lock_guard<decltype(write_mutex)> lock{write_mutex};
	const ctx::uninterruptible::nothrow ui;
	throw_on_error
	{
		d.d->IngestExternalFile(c, files, opts)
	};
}

void
ircd::db::del(column &column,
              const string_view &key,
              const sopts &sopts)
{
	database &d(column);
	database::column &c(column);
	auto opts(make_opts(sopts));

	const std::lock_guard<decltype(write_mutex)> lock{write_mutex};
	const ctx::uninterruptible::nothrow ui;
	log::debug
	{
		log, "'%s' %lu '%s' DELETE key(%zu B)",
		name(d),
		sequence(d),
		name(c),
		key.size()
	};

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
	database &d(column);
	database::column &c(column);
	auto opts(make_opts(sopts));

	const std::lock_guard<decltype(write_mutex)> lock{write_mutex};
	const ctx::uninterruptible::nothrow ui;
	log::debug
	{
		log, "'%s' %lu '%s' PUT key(%zu B) val(%zu B)",
		name(d),
		sequence(d),
		name(c),
		size(key),
		size(val)
	};

	throw_on_error
	{
		d.d->Put(opts, c, slice(key), slice(val))
	};
}

void
ircd::db::prefetch(column &column,
                   const string_view &key,
                   const gopts &gopts)
{
	if(exists(cache(column), key))
		return;

	request([column(column), key(std::string(key)), gopts]
	() mutable
	{
		has(column, key, gopts);
	});
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

	// Perform a co-RP query to the filtration
	// NOTE disabled for rocksdb >= v5.15 due to a regression
	// where rocksdb does not init SuperVersion data in the column
	// family handle and this codepath triggers null derefs and ub.
	if(0 && c.table_opts.filter_policy)
	{
		const auto k(slice(key));
		auto opts(make_opts(gopts));
		opts.read_tier = NON_BLOCKING;
		thread_local std::string discard;
		if(!d.d->KeyMayExist(opts, c, k, &discard, nullptr))
			return false;
	}

	const auto it
	{
		seek(column, key, gopts)
	};

	return valid_eq(*it, key);
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
const descriptor &()
const
{
	assert(c->descriptor);
	return *c->descriptor;
}

//
// column::const_iterator
//

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
	return seek(c, p, it.opts, it.it);
}
template bool ircd::db::seek<ircd::db::pos>(column::const_iterator_base &, const pos &);
template bool ircd::db::seek<ircd::string_view>(column::const_iterator_base &, const string_view &);

///////////////////////////////////////////////////////////////////////////////
//
// comparator.h
//

//
// linkage placements for integer comparators so they all have the same addr
//

ircd::db::cmp_int64_t::cmp_int64_t()
{
}

ircd::db::cmp_int64_t::~cmp_int64_t()
noexcept
{
}

ircd::db::cmp_uint64_t::cmp_uint64_t()
{
}

ircd::db::cmp_uint64_t::~cmp_uint64_t()
noexcept
{
}

ircd::db::reverse_cmp_int64_t::reverse_cmp_int64_t()
{
}

ircd::db::reverse_cmp_int64_t::~reverse_cmp_int64_t()
noexcept
{
}

ircd::db::reverse_cmp_uint64_t::reverse_cmp_uint64_t()
{
}

ircd::db::reverse_cmp_uint64_t::~reverse_cmp_uint64_t()
noexcept
{
}

//
// cmp_string_view
//

ircd::db::cmp_string_view::cmp_string_view()
:db::comparator{"string_view", &less, &equal}
{
}

bool
ircd::db::cmp_string_view::less(const string_view &a,
                                const string_view &b)
{
	return a < b;
}

bool
ircd::db::cmp_string_view::equal(const string_view &a,
                                 const string_view &b)
{
	return a == b;
}

//
// reverse_cmp_string_view
//

ircd::db::reverse_cmp_string_view::reverse_cmp_string_view()
:db::comparator{"reverse_string_view", &less, &equal}
{
}

bool
ircd::db::reverse_cmp_string_view::less(const string_view &a,
                                        const string_view &b)
{
	/// RocksDB sez things will not work correctly unless a shorter string
	/// result returns less than a longer string even if one intends some
	/// reverse ordering
	if(a.size() < b.size())
		return true;

	/// Furthermore, b.size() < a.size() returning false from this function
	/// appears to not be correct. The reversal also has to also come in
	/// the form of a bytewise forward iteration.
	return std::memcmp(a.data(), b.data(), std::min(a.size(), b.size())) > 0;
}

bool
ircd::db::reverse_cmp_string_view::equal(const string_view &a,
                                         const string_view &b)
{
	return a == b;
}

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
	static bool _seek(database::column &, const pos &, const rocksdb::ReadOptions &, rocksdb::Iterator &it);
	static bool _seek(database::column &, const string_view &, const rocksdb::ReadOptions &, rocksdb::Iterator &it);
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
	const rocksdb::ReadOptions opts
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
	const ctx::uninterruptible::nothrow ui;

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
ircd::db::pinned(const rocksdb::Cache *const &cache)
{
	return cache? pinned(*cache) : 0;
}

size_t
ircd::db::pinned(const rocksdb::Cache &cache)
{
	return cache.GetPinnedUsage();
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

uint64_t
ircd::db::ticker(const rocksdb::Cache *const &cache,
                 const uint32_t &ticker_id)
{
	return cache? ticker(*cache, ticker_id) : 0UL;
}

const uint64_t &
ircd::db::ticker(const rocksdb::Cache &cache,
                 const uint32_t &ticker_id)
{
	const auto &c
	{
		dynamic_cast<const database::cache &>(cache)
	};

	static const uint64_t &zero
	{
		0ULL
	};

	return c.stats?
		c.stats->ticker.at(ticker_id):
		zero;
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
	const ctx::uninterruptible::nothrow ui;

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

rocksdb::CompressionType
ircd::db::find_supported_compression(const std::string &list)
{
	rocksdb::CompressionType ret
	{
		rocksdb::kNoCompression
	};

	tokens(list, ';', [&ret]
	(const string_view &name)
	{
		if(ret != rocksdb::kNoCompression)
			return;

		for(size_t i(0); i < db::compressions.size(); ++i)
			if(!db::compressions.at(i).empty())
				if(name == db::compressions.at(i))
				{
					ret = rocksdb::CompressionType(i);
					break;
				}
	});

	return ret;
}

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

/// Convert our options structure into RocksDB's options structure.
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

/// Update a RocksDB options structure with our options structure. We use
/// operator+= for fun here; we can avoid reconstructing and returning a new
/// options structure in some cases by breaking out this function from
/// make_opts().
rocksdb::ReadOptions &
ircd::db::operator+=(rocksdb::ReadOptions &ret,
                     const gopts &opts)
{
	ret.pin_data = test(opts, get::PIN);
	ret.fill_cache |= test(opts, get::CACHE);
	ret.fill_cache &= !test(opts, get::NO_CACHE);
	ret.tailing = test(opts, get::NO_SNAPSHOT);
	ret.prefix_same_as_start = test(opts, get::PREFIX);
	ret.total_order_seek = test(opts, get::ORDERED);
	ret.verify_checksums = bool(read_checksum);
	ret.verify_checksums |= test(opts, get::CHECKSUM);
	ret.verify_checksums &= !test(opts, get::NO_CHECKSUM);

	ret.readahead_size = opts.readahead;
	ret.iter_start_seqnum = opts.seqnum;

	if(opts.snapshot && !test(opts, get::NO_SNAPSHOT))
		ret.snapshot = opts.snapshot;

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
		if(!fs::is_dir(dir))
			continue;

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

	const string_view parts[]
	{
		prefix, name, lex_cast(checkpoint)
	};

	return fs::make_path(parts);
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
ircd::db::reflect(const rocksdb::WriteStallCondition &c)
{
	using rocksdb::WriteStallCondition;

	switch(c)
	{
		case WriteStallCondition::kNormal:   return "NORMAL";
		case WriteStallCondition::kDelayed:  return "DELAYED";
		case WriteStallCondition::kStopped:  return "STOPPED";
	}

	return "??????";
}

ircd::string_view
ircd::db::reflect(const rocksdb::BackgroundErrorReason &r)
{
	using rocksdb::BackgroundErrorReason;

	switch(r)
	{
		case BackgroundErrorReason::kFlush:          return "FLUSH";
		case BackgroundErrorReason::kCompaction:     return "COMPACTION";
		case BackgroundErrorReason::kWriteCallback:  return "WRITE";
		case BackgroundErrorReason::kMemTable:       return "MEMTABLE";
	}

	return "??????";
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
ircd::db::reflect(const rocksdb::Env::WriteLifeTimeHint &h)
{
	using WriteLifeTimeHint = rocksdb::Env::WriteLifeTimeHint;

	switch(h)
	{
		case WriteLifeTimeHint::WLTH_NOT_SET:   return "NOT_SET";
		case WriteLifeTimeHint::WLTH_NONE:      return "NONE";
		case WriteLifeTimeHint::WLTH_SHORT:     return "SHORT";
		case WriteLifeTimeHint::WLTH_MEDIUM:    return "MEDIUM";
		case WriteLifeTimeHint::WLTH_LONG:      return "LONG";
		case WriteLifeTimeHint::WLTH_EXTREME:   return "EXTREME";
	}

	return "WLTH_????"_sv;
}

ircd::string_view
ircd::db::reflect(const rocksdb::Status::Severity &s)
{
	using Severity = rocksdb::Status::Severity;

	switch(s)
	{
		case Severity::kNoError:             return "NONE";
		case Severity::kSoftError:           return "SOFT";
		case Severity::kHardError:           return "HARD";
		case Severity::kFatalError:          return "FATAL";
		case Severity::kUnrecoverableError:  return "UNRECOVERABLE";
		case Severity::kMaxSeverity:         break;
	}

	return "?????";
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
