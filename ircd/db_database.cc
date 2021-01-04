// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include "db.h"

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
/// IRCd's applications are NOT tolerant of skip recovery. You will create an
/// incoherent database. NEVER USE "skip" RECOVERY MODE.
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

/// Conf item determines if database repair should occur (before open). This
/// mechanism can be used when SST file corruption occurs which is too deep
/// for log-based recovery. The affected blocks may be discarded; this risks
/// destabilizing an application expecting the data in those blocks to exist.
///
/// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
///
/// Use with caution.
///
/// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
///
decltype(ircd::db::open_repair)
ircd::db::open_repair
{
	{ "name",     "ircd.db.open.repair"  },
	{ "default",  false                  },
	{ "persist",  false                  },
};

/// Conf item toggles whether automatic compaction is enabled or disabled for
/// all databases upon opening. This is useful for developers, debugging and
/// valgrind etc to prevent these background jobs from spawning when unwanted.
decltype(ircd::db::auto_compact)
ircd::db::auto_compact
{
	{ "name",     "ircd.db.compact.auto" },
	{ "default",  true                   },
	{ "persist",  false                  },
};

/// Conf item toggles whether rocksdb is allowed to perform file deletion and
/// garbage collection operations as normal. This can be prevented for
/// diagnostic and safemode purposes.
decltype(ircd::db::auto_deletion)
ircd::db::auto_deletion
{
	{ "name",     "ircd.db.deletion.auto" },
	{ "default",  true                    },
	{ "persist",  false                   },
};

/// Conf item dictates whether databases will be opened in slave mode; this
/// is a recent feature of RocksDB which may not be available. It allows two
/// instances of a database, so long as only one is not opened as a slave.
decltype(ircd::db::open_slave)
ircd::db::open_slave
{
	{ "name",     "ircd.db.open.slave" },
	{ "default",  false                },
	{ "persist",  false                },
};

void
ircd::db::sync(database &d)
{
	log::debug
	{
		log, "[%s] @%lu SYNC WAL",
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
	log::debug
	{
		log, "[%s] @%lu FLUSH WAL",
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
               const bool &blocking,
               const bool &now)
{
	for(const auto &c : d.columns)
	{
		db::column column{*c};
		db::sort(column, blocking, now);
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

	for(const auto &c : d.columns) try
	{
		db::column column{*c};
		compact(column, range, -1, cb);
	}
	catch(const ctx::interrupted &)
	{
		throw;
	}
	catch(const std::exception &e)
	{
		assert(c);
		log::error
		{
			log, "[%s] compact '%s' :%s",
			name(d),
			name(*c),
			e.what(),
		};
	}
}

void
ircd::db::compact(database &d,
                  const std::pair<int, int> &level,
                  const compactor &cb)
{
	for(const auto &c : d.columns) try
	{
		db::column column{*c};
		compact(column, level, cb);
	}
	catch(const ctx::interrupted &)
	{
		throw;
	}
	catch(const std::exception &e)
	{
		assert(c);
		log::error
		{
			log, "[%s] compact '%s' :%s",
			name(d),
			name(*c),
			e.what(),
		};
	}
}

void
ircd::db::check(database &d)
{
	assert(d.d);
	throw_on_error
	{
		d.d->VerifyChecksum()
	};
}

void
ircd::db::check(database &d,
                const string_view &file)
{
	assert(file);
	assert(d.d);

	const auto &opts
	{
		d.d->GetOptions()
	};

	const rocksdb::EnvOptions env_opts
	{
		opts
	};

	const bool absolute
	{
		fs::is_absolute(file)
	};

	const string_view parts[]
	{
		d.path, file
	};

	const std::string path
	{
		!absolute?
			fs::path_string(parts):
			std::string{file}
	};

	throw_on_error
	{
		rocksdb::VerifySstFileChecksum(opts, env_opts, path)
	};
}

void
ircd::db::resume(database &d)
{
	assert(d.d);
	const ctx::uninterruptible::nothrow ui;
	const std::lock_guard lock
	{
		d.write_mutex
	};

	const auto errors
	{
		db::errors(d)
	};

	log::debug
	{
		log, "[%s] Attempting to resume from %zu errors @%lu",
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
		log, "[%s] Resumed normal operation at sequence number %lu; cleared %zu errors",
		name(d),
		sequence(d),
		errors.size()
	};
}

void
ircd::db::refresh(database &d)
{
	assert(d.d);

	throw_on_error
	{
		#ifdef IRCD_DB_HAS_SECONDARY
		d.d->TryCatchUpWithPrimary()
		#else
		rocksdb::Status::NotSupported(slice("Slave mode not supported by this RocksDB"_sv))
		#endif
	};

	log::debug
	{
		log, "[%s] Caught up with primary database.",
		name(d)
	};
}

void
ircd::db::bgpause(database &d)
{
	assert(d.d);

	throw_on_error
	{
		d.d->PauseBackgroundWork()
	};

	log::debug
	{
		log, "[%s] Paused all background work",
		name(d)
	};
}

void
ircd::db::bgcontinue(database &d)
{
	assert(d.d);

	log::debug
	{
		log, "[%s] Continuing background work",
		name(d)
	};

	throw_on_error
	{
		d.d->ContinueBackgroundWork()
	};
}

void
ircd::db::bgcancel(database &d,
                   const bool &blocking)
{
	assert(d.d);
	log::debug
	{
		log, "[%s] Canceling all background work...",
		name(d)
	};

	rocksdb::CancelAllBackgroundWork(d.d.get(), blocking);
	if(!blocking)
		return;

	assert(d.env);
	assert(d.env->st);
	const ctx::uninterruptible::nothrow ui;
	for(auto &pool : d.env->st->pool) if(pool)
	{
		log::debug
		{
			log, "[%s] Waiting for tasks:%zu queued:%zu active:%zu in pool '%s'",
			name(d),
			pool->tasks.size(),
			pool->p.pending(),
			pool->p.active(),
			ctx::name(pool->p),
		};

		pool->wait();
	}

	const auto errors
	{
		property<uint64_t>(d, rocksdb::DB::Properties::kBackgroundErrors)
	};

	const auto level
	{
		errors? log::level::ERROR : log::level::DEBUG
	};

	log::logf
	{
		log, level,
		"[%s] Canceled all background work; errors:%lu",
		name(d),
		errors
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

	const std::lock_guard lock{d.write_mutex};
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
		log, "[%s] Checkpoint at sequence %lu in `%s' complete",
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

	throw_on_error
	{
		d.d->SetDBOptions(options)
	};
}

/// Set the rdb logging level by translating our ircd::log::level to the
/// RocksDB enum. This translation is a reasonable convenience, as both
/// enums are similar enough.
void
ircd::db::loglevel(database &d,
                   const ircd::log::level &fac)
{
	using ircd::log::level;

	rocksdb::InfoLogLevel lev
	{
		rocksdb::WARN_LEVEL
	};

	switch(fac)
	{
		case level::CRITICAL:  lev = rocksdb::FATAL_LEVEL;   break;
		case level::ERROR:     lev = rocksdb::ERROR_LEVEL;   break;
		case level::WARNING:
		case level::NOTICE:    lev = rocksdb::WARN_LEVEL;    break;
		case level::INFO:      lev = rocksdb::INFO_LEVEL;    break;
		case level::DERROR:
		case level::DWARNING:
		case level::DEBUG:     lev = rocksdb::DEBUG_LEVEL;   break;
		case level::_NUM_:     assert(0);                    break;
	}

	d.logger->SetInfoLogLevel(lev);
}

/// Set the rdb logging level by translating our ircd::log::level to the
/// RocksDB enum. This translation is a reasonable convenience, as both
/// enums are similar enough.
ircd::log::level
ircd::db::loglevel(const database &d)
{
	const auto &level
	{
		d.logger->GetInfoLogLevel()
	};

	switch(level)
	{
		default:
		case rocksdb::NUM_INFO_LOG_LEVELS:
			assert(0);

		case rocksdb::HEADER_LEVEL:
		case rocksdb::FATAL_LEVEL:     return log::level::CRITICAL;
		case rocksdb::ERROR_LEVEL:     return log::level::ERROR;
		case rocksdb::WARN_LEVEL:      return log::level::WARNING;
		case rocksdb::INFO_LEVEL:      return log::level::INFO;
		case rocksdb::DEBUG_LEVEL:     return log::level::DEBUG;
	}
}

ircd::db::options
ircd::db::getopt(const database &d)
{
	return options
	{
		d.d->GetDBOptions()
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
		mutable_cast(cd)
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
	auto &d(mutable_cast(cd));
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
	database &d(mutable_cast(cd));
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
	database &d(mutable_cast(cd));
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
decltype(ircd::util::instance_list<ircd::db::database>::allocator)
ircd::util::instance_list<ircd::db::database>::allocator
{};

template<>
decltype(ircd::util::instance_list<ircd::db::database>::list)
ircd::util::instance_list<ircd::db::database>::list
{
	allocator
};

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
	db::open_repair
}
,slave
{
	db::open_slave
}
,read_only
{
	slave || ircd::read_only
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
,wal_filter
{
	std::make_unique<struct wal_filter>(this)
}
,rate_limiter
{
	std::make_unique<struct rate_limiter>(this)
}
,allocator
{
	#ifdef IRCD_DB_HAS_ALLOCATOR
	std::make_shared<struct allocator>(this, nullptr, database::allocator::cache_arena)
	#endif
}
,ssts{rocksdb::NewSstFileManager
(
	env.get(),   // env
	logger,      // logger
	{},          // trash_dir
	0,           // rate_bytes_per_sec
	true,        // delete_existing_trash
	nullptr,     // Status*
	0.05,        // max_trash_db_ratio 0.25
	64_MiB       // bytes_max_delete_chunk
)}
,row_cache
{
	std::make_shared<database::cache>
	(
		this, this->stats, this->allocator, this->name, 16_MiB
	)
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

	// limit maxfdto prevent too many small files degrading read perf; too low is
	// bad for write perf.
	opts->max_open_files = !slave?
		fs::support::rlimit_nofile():
		-1;

	// MUST be 0 or std::threads are spawned in rocksdb.
	opts->max_file_opening_threads = 0;

	opts->max_background_jobs = 16;
	opts->max_background_flushes = 8;
	opts->max_background_compactions = 4;
	opts->max_subcompactions = 1;

	// For the write-side of a compaction process: writes will be of approx
	// this size. The compaction process is composing a buffer of this size
	// between those writes. Too large a buffer will hog the CPU and starve
	// other ircd::ctx's. Too small a buffer will be inefficient.
	opts->writable_file_max_buffer_size = 2_MiB; //TODO: conf

	// For the read-side of the compaction process.
	opts->compaction_readahead_size = !opts->use_direct_reads?
		2_MiB: //TODO: conf
		0;

	opts->max_total_wal_size = 96_MiB;
	opts->db_write_buffer_size = 96_MiB;

	//TODO: range_sync
	opts->bytes_per_sync = 0;
	opts->wal_bytes_per_sync = 0;

	// This prevents the creation of additional SST files and lots of I/O on
	// either DB open and close.
	opts->avoid_flush_during_recovery = true;
	opts->avoid_flush_during_shutdown = false;

	opts->allow_concurrent_memtable_write = true;
	opts->enable_write_thread_adaptive_yield = false;
	opts->enable_pipelined_write = false;
	opts->write_thread_max_yield_usec = 0;
	opts->write_thread_slow_yield_usec = 0;

	// Doesn't appear to be in effect when direct io is used. Not supported by
	// all filesystems so disabled for now.
	// TODO: use fs::support::test_fallocate() test similar to direct_io_test_file.
	opts->allow_fallocate = false;

	// Detect if O_DIRECT is possible if db::init left a file in the
	// database directory claiming such. User can force no direct io
	// with program option at startup (i.e -nodirect).
	opts->use_direct_reads = bool(fs::fd::opts::direct_io_enable)?
		fs::exists(init::direct_io_test_file_path):
		false;

	// Use the determined direct io value for writes as well.
	//opts->use_direct_io_for_flush_and_compaction = opts->use_direct_reads;

	// Default corruption tolerance is zero-tolerance; db fails to open with
	// error by default to inform the user. The rest of the options are
	// various relaxations for how to proceed.
	opts->wal_recovery_mode = rocksdb::WALRecoveryMode::kAbsoluteConsistency;

	// When corrupted after crash, the DB is rolled back before the first
	// corruption and erases everything after it, giving a consistent
	// state up at that point, though losing some recent data.
	if(string_view(open_recover) == "point")
		opts->wal_recovery_mode = rocksdb::WALRecoveryMode::kPointInTimeRecovery;

	// When corrupted after crash and PointInTimeRecovery does not work,
	// this will drop more data, but consistently. RocksDB sez the WAL is not
	// used at all in this mode.
	#if ROCKSDB_MAJOR > 6 \
	|| (ROCKSDB_MAJOR == 6 && ROCKSDB_MINOR >= 10)
	if(string_view(open_recover) == "recover")
		opts->best_efforts_recovery = true;
	#endif

	// Skipping corrupted records will create gaps in the DB timeline where the
	// application (like a matrix timeline) cannot tolerate the unexpected gap.
	if(string_view(open_recover) == "skip" || string_view(open_recover) == "recover")
		opts->wal_recovery_mode = rocksdb::WALRecoveryMode::kSkipAnyCorruptedRecords;

	// Tolerating corrupted records is very last-ditch for getting the database to
	// open in a catastrophe. We have no use for this option but should use it for
	//TODO: emergency salvage-mode.
	if(string_view(open_recover) == "tolerate")
		opts->wal_recovery_mode = rocksdb::WALRecoveryMode::kTolerateCorruptedTailRecords;

	// Setup env
	opts->env = env.get();

	// Setup WAL filter
	opts->wal_filter = this->wal_filter.get();

	// Setup Rate Limiter
	opts->rate_limiter = this->rate_limiter;

	// Setup SST file mgmt
	opts->sst_file_manager = this->ssts;

	// Setup row cache.
	opts->row_cache = this->row_cache;

	// Setup logging
	logger->SetInfoLogLevel(ircd::debugmode? rocksdb::DEBUG_LEVEL : rocksdb::WARN_LEVEL);
	opts->info_log_level = logger->GetInfoLogLevel();
	opts->info_log = logger;
	opts->keep_log_file_num = 1;
	//opts->max_log_file_size = 32_MiB;

	// Setup event and statistics callbacks
	opts->listeners.emplace_back(this->events);

	// Setup histogram collecting
	#if ROCKSDB_MAJOR > 6 \
	|| (ROCKSDB_MAJOR == 6 && ROCKSDB_MINOR >= 1)
		//this->stats->set_stats_level(rocksdb::kExceptTimeForMutex);
		this->stats->set_stats_level(rocksdb::kAll);
	#else
		//this->stats->stats_level_ = rocksdb::kExceptTimeForMutex;
		this->stats->stats_level_ = rocksdb::kAll;
	#endif

	opts->stats_dump_period_sec = 0; // Disable noise
	opts->statistics = this->stats;

	#ifdef RB_DEBUG
	opts->dump_malloc_stats = true;
	#endif

	// Disables the timer to delete unused files; this operation occurs
	// instead with our compaction operations so we don't need to complicate.
	opts->delete_obsolete_files_period_micros = 0;

	// Uses thread_local counters in rocksdb and probably useless for ircd::ctx.
	opts->enable_thread_tracking = false;

	// Setup performance metric options
	//rocksdb::SetPerfLevel(rocksdb::PerfLevel::kDisable);

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

	if(!existing.empty())
		throw error
		{
			"Failed to describe existing column '%s' (and %zd others...)",
			*begin(existing),
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
	if(opts->create_if_missing && !fs::is_dir(path) && !ircd::write_avoid)
		fs::mkdir(path);

	// Announce attempt before usual point where exceptions are thrown
	log::info
	{
		log, "Opening database \"%s\" @ `%s' with %zu columns...",
		this->name,
		path,
		columns.size()
	};

	if(read_only)
		log::warning
		{
			log, "Database \"%s\" @ `%s' will be opened in read-only mode.",
			this->name,
			path,
		};

	// Open DB into ptr
	rocksdb::DB *ptr;
	if(slave)
		throw_on_error
		{
			#ifdef IRCD_DB_HAS_SECONDARY
			rocksdb::DB::OpenAsSecondary(*opts, path, "/tmp/slave", columns, &handles, &ptr)
			#else
			rocksdb::Status::NotSupported(slice("Slave mode not supported by this RocksDB"_sv))
			#endif
		};
	else if(read_only)
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
			log, "[%s] Error finding described handle '%s' which RocksDB opened :%s",
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
	std::string ret;
	throw_on_error
	{
		d->GetDbIdentity(ret)
	};

	return ret;
}()}
,checkpointer{[this]
{
	rocksdb::Checkpoint *checkpointer{nullptr};
	throw_on_error
	{
		rocksdb::Checkpoint::Create(this->d.get(), &checkpointer)
	};

	return checkpointer;
}()}
{
	// Disable file deletions here if ordered by the conf item (generally for
	// -safe mode operation). If this can be done via DBOptions rather than
	// here it would be better.
	if(!db::auto_deletion)
		db::fdeletions(*this, false);

	// Conduct drops from schema changes. The database must be fully opened
	// as if they were not dropped first, then we conduct the drop operation
	// here. The drop operation has no effects until the database is next
	// closed; the dropped columns will still work during this instance.
	for(const auto &colptr : columns)
		if(describe(*colptr).drop)
			db::drop(*colptr);

	// Database integrity check branch.
	if(has(string_view(ircd::diagnostic), "checkdb"))
	{
		log::notice
		{
			log, "[%s] Verifying database integrity. This may take several minutes...",
			this->name
		};

		check(*this);
	}

	log::info
	{
		log, "[%s] Opened database @ `%s' with %zu columns at sequence number %lu.",
		this->name,
		path,
		columns.size(),
		d->GetLatestSequenceNumber()
	};
}
catch(const error &e)
{
	log::error
	{
		"Error opening db [%s] %s",
		name,
		e.what()
	};

	throw;
}
catch(const std::exception &e)
{
	log::error
	{
		"Error opening db [%s] %s",
		name,
		e.what()
	};

	throw error
	{
		"Failed to open db [%s] %s",
		name,
		e.what()
	};
}

ircd::db::database::~database()
noexcept try
{
	const ctx::uninterruptible::nothrow ui;
	const std::unique_lock lock
	{
		write_mutex
	};

	log::info
	{
		log, "[%s] closing database @ `%s'...",
		name,
		path
	};

	if(likely(prefetcher))
	{
		const size_t canceled
		{
			prefetcher->cancel(*this)
		};

		log::debug
		{
			log, "[%s] canceled %zu queued prefetches; waiting for any pending ...",
			name,
			canceled,
		};

		// prefetcher::cancel() only removes requests from its queue, but if
		// a prefetch request from this database is in flight that is bad; so
		// we wait until the unit has completed its pending requests.
		prefetcher->wait_pending();
	}

	bgcancel(*this, true);

	log::debug
	{
		log, "[%s] closing columns...",
		name
	};

	this->checkpointer.reset(nullptr);
	this->column_names.clear();
	this->column_index.clear();
	this->columns.clear();
	log::debug
	{
		log, "[%s] closed columns; flushing...",
		name
	};

	if(!read_only)
		flush(*this);

	log::debug
	{
		log, "[%s] flushed; synchronizing...",
		name
	};

	if(!read_only)
		sync(*this);

	log::debug
	{
		log, "[%s] synchronized with hardware.",
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

	env->st.reset(nullptr);

	log::info
	{
		log, "[%s] closed database @ `%s' at sequence number %lu.",
		name,
		path,
		sequence
	};
}
catch(const std::exception &e)
{
	log::error
	{
		log, "Error closing database(%p) :%s",
		this,
		e.what()
	};

	return;
}
catch(...)
{
	log::critical
	{
		log, "Unknown error closing database(%p)",
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
	return operator[](cfid(name));
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
	throw not_found
	{
		"[%s] column id[%u] is not available or specified in schema",
		this->name,
		id
	};
}

const ircd::db::database::column &
ircd::db::database::operator[](const string_view &name)
const
{
	return operator[](cfid(name));
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
	throw not_found
	{
		"[%s] column id[%u] is not available or specified in schema",
		this->name,
		id
	};
}

uint32_t
ircd::db::database::cfid(const string_view &name)
const
{
	const int32_t id
	{
		cfid(std::nothrow, name)
	};

	if(id < 0)
		throw not_found
		{
			"[%s] column '%s' is not available or specified in schema",
			this->name,
			name
		};

	return id;
}

int32_t
ircd::db::database::cfid(const std::nothrow_t,
                         const string_view &name)
const
{
	const auto it{column_names.find(name)};
	return it != std::end(column_names)?
		db::id(*it->second):
		-1;
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
	log::debug
	{
		log, "[%s]'%s' @%lu DROPPING COLUMN",
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
		log, "[%s]'%s' @%lu DROPPED COLUMN",
		name(d),
		name(c),
		sequence(d)
	};
}

bool
ircd::db::dropped(const database::column &c)
{
	return c.descriptor?
		c.descriptor->drop:
		true;
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
	descriptor.name, db::options{descriptor.options}
)
,d{&d}
,descriptor{&descriptor}
,key_type{this->descriptor->type.first}
,mapped_type{this->descriptor->type.second}
,cmp{this->d, this->descriptor->cmp}
,prefix{this->d, this->descriptor->prefix}
,cfilter{this, this->descriptor->compactor}
,stall{rocksdb::WriteStallCondition::kNormal}
,stats
{
	descriptor.name != "default"s?
		std::make_shared<struct database::stats>(this->d, this):
		this->d->stats
}
,allocator
{
	#ifdef IRCD_DB_HAS_ALLOCATOR
	std::make_shared<struct database::allocator>(this->d, this, database::allocator::cache_arena, descriptor.block_size)
	#endif
}
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

	// Set filter reductions for this column. This means we expect a key to exist.
	this->options.optimize_filters_for_hits = this->descriptor->expect_queries_hit;

	static const long write_buffer_size_limit[]
	{
		256_KiB, 16_MiB
	};

	// Derive the write buffer size from the block size
	this->options.write_buffer_size = std::clamp
	(
		long(this->descriptor->write_buffer_blocks * this->descriptor->block_size),
		write_buffer_size_limit[0],
		write_buffer_size_limit[1]
	);

	this->options.max_write_buffer_number = 12;
	this->options.min_write_buffer_number_to_merge = 2;
	this->options.max_write_buffer_number_to_maintain = 0;
	#if ROCKSDB_MAJOR > 6 \
	|| (ROCKSDB_MAJOR == 6 && ROCKSDB_MINOR > 5) \
	|| (ROCKSDB_MAJOR == 6 && ROCKSDB_MINOR == 5 && ROCKSDB_PATCH >= 2)
		this->options.max_write_buffer_size_to_maintain = 0; //this->options.write_buffer_size * 4;
	#endif

	this->options.arena_block_size = std::clamp
	(
		this->options.write_buffer_size / 2L,
		ulong(512_KiB),
		ulong(4_MiB)
	);

	// Conf item can be set to disable automatic compactions. For developers
	// and debugging; good for valgrind.
	this->options.disable_auto_compactions = !bool(db::auto_compact);

	// Set the compaction style; we don't override this in the descriptor yet.
	//this->options.compaction_style = rocksdb::kCompactionStyleNone;
	//this->options.compaction_style = rocksdb::kCompactionStyleLevel;
	//this->options.compaction_style = rocksdb::kCompactionStyleUniversal;
	this->options.compaction_style =
		this->descriptor->compaction_pri.empty() || this->descriptor->compaction_pri == "Universal"?
			rocksdb::kCompactionStyleUniversal:
			rocksdb::kCompactionStyleLevel;

	// Set the compaction priority from string in the descriptor
	this->options.compaction_pri =
		this->descriptor->compaction_pri == "kByCompensatedSize"?
			rocksdb::CompactionPri::kByCompensatedSize:
		this->descriptor->compaction_pri == "kMinOverlappingRatio"?
			rocksdb::CompactionPri::kMinOverlappingRatio:
		this->descriptor->compaction_pri == "kOldestSmallestSeqFirst"?
			rocksdb::CompactionPri::kOldestSmallestSeqFirst:
		this->descriptor->compaction_pri == "kOldestLargestSeqFirst"?
			rocksdb::CompactionPri::kOldestLargestSeqFirst:
			rocksdb::CompactionPri::kOldestLargestSeqFirst;

	// RocksDB sez:
	// stop_writes_trigger >= slowdown_writes_trigger >= file_num_compaction_trigger

	this->options.level0_stop_writes_trigger =
		this->options.compaction_style == rocksdb::kCompactionStyleUniversal?
			(this->options.max_write_buffer_number * 8):
			64;

	this->options.level0_slowdown_writes_trigger =
		this->options.compaction_style == rocksdb::kCompactionStyleUniversal?
			(this->options.max_write_buffer_number * 6):
			48;

	this->options.level0_file_num_compaction_trigger =
		this->options.compaction_style == rocksdb::kCompactionStyleUniversal?
			(this->options.max_write_buffer_number * 2):
			4;

	// Universal compaction mode options
	auto &universal(this->options.compaction_options_universal);
	//universal.stop_style = rocksdb::kCompactionStopStyleSimilarSize;
	universal.stop_style = rocksdb::kCompactionStopStyleTotalSize;
	universal.allow_trivial_move = false;
	universal.compression_size_percent = -1;
	universal.max_size_amplification_percent = 6667;
	universal.size_ratio = 36;
	universal.min_merge_width = 8;
	universal.max_merge_width = 4 * universal.min_merge_width;

	// Level compaction mode options
	this->options.num_levels = 7;
	this->options.level_compaction_dynamic_level_bytes = false;
	this->options.target_file_size_base = this->descriptor->target_file_size.base;
	this->options.target_file_size_multiplier = this->descriptor->target_file_size.multiplier;
	this->options.max_bytes_for_level_base = this->descriptor->max_bytes_for_level[0].base;
	this->options.max_bytes_for_level_multiplier = this->descriptor->max_bytes_for_level[0].multiplier;
	this->options.max_bytes_for_level_multiplier_additional = std::vector<int>(this->options.num_levels, 1);
	{
		auto &dst(this->options.max_bytes_for_level_multiplier_additional);
		const auto &src(this->descriptor->max_bytes_for_level);
		const size_t src_size(std::distance(begin(src) + 1, std::end(src)));
		assert(src_size >= 1);
		const auto end
		{
			begin(src) + 1 + std::min(dst.size(), src_size)
		};

		std::transform(begin(src) + 1, end, begin(dst), []
		(const auto &mbfl)
		{
			return mbfl.multiplier;
		});
	}

	//this->options.ttl = -2U;
	#ifdef IRCD_DB_HAS_PERIODIC_COMPACTIONS
	this->options.periodic_compaction_seconds = this->descriptor->compaction_period.count();
	#endif

	// Compression
	const auto &[_compression_algos, _compression_opts]
	{
		split(this->descriptor->compression, ' ')
	};

	this->options.compression = find_supported_compression(_compression_algos);
	//this->options.compression = rocksdb::kNoCompression;

	// Compression options
	this->options.compression_opts.enabled = true;
	this->options.compression_opts.max_dict_bytes = 0; // ??
	if(this->options.compression == rocksdb::kZSTD)
		this->options.compression_opts.level = -3;

	// Bottommost compression
	this->options.bottommost_compression = this->options.compression;
	this->options.bottommost_compression_opts = this->options.compression_opts;
	if(this->options.bottommost_compression == rocksdb::kZSTD)
		this->options.bottommost_compression_opts.level = 0;

	//
	// Table options
	//

	// Block based table index type.
	if constexpr(ROCKSDB_MAJOR > 6 || (ROCKSDB_MAJOR == 6 && ROCKSDB_MINOR >= 6))
		table_opts.format_version = 5; // RocksDB >= 6.6.x compat only; otherwise use 4
	else
		table_opts.format_version = 4; // RocksDB >= 5.16.x compat only; otherwise use 3.

	table_opts.index_type = rocksdb::BlockBasedTableOptions::kTwoLevelIndexSearch;
	table_opts.read_amp_bytes_per_bit = 8;

	// Delta encoding is always used (option ignored) for table
	// format_version >= 4 unless block_align=true.
	table_opts.use_delta_encoding = false;
	table_opts.block_restart_interval = 8;
	table_opts.index_block_restart_interval = 1; // >1 slows down iterations

	// Determine whether the index for this column should be compressed.
	const bool is_string_index(this->descriptor->type.first == typeid(string_view));
	const bool is_compression(this->options.compression != rocksdb::kNoCompression);
	table_opts.enable_index_compression = is_compression; //&& is_string_index;

	// Setup the block size
	table_opts.block_size = this->descriptor->block_size;
	table_opts.metadata_block_size = this->descriptor->meta_block_size;
	table_opts.block_size_deviation = 50;

	// Block alignment doesn't work if compression is enabled for this
	// column. If not, we want block alignment for direct IO.
	table_opts.block_align =
		this->options.compression == rocksdb::kNoCompression ||
		this->options.compression == rocksdb::kDisableCompressionOption;

	//table_opts.data_block_index_type = rocksdb::BlockBasedTableOptions::kDataBlockBinaryAndHash;
	//table_opts.data_block_hash_table_util_ratio = 0.75;

	// Specify that index blocks should use the cache. If not, they will be
	// pre-read into RAM by rocksdb internally. Because of the above
	// TwoLevelIndex + partition_filters configuration on RocksDB v5.15 it's
	// better to use pre-read except in the case of a massive database.
	table_opts.cache_index_and_filter_blocks = true;
	table_opts.cache_index_and_filter_blocks_with_high_priority = true;
	table_opts.pin_top_level_index_and_filter = false;
	table_opts.pin_l0_filter_and_index_blocks_in_cache = false;
	table_opts.partition_filters = true;

	// Setup the cache for assets.
	const auto &cache_size(this->descriptor->cache_size);
	if(cache_size != 0)
		table_opts.block_cache = std::make_shared<database::cache>(this->d, this->stats, this->allocator, this->name, cache_size);

	// RocksDB will create an 8_MiB block_cache if we don't create our own.
	// To honor the user's desire for a zero-size cache, this must be set.
	if(!table_opts.block_cache)
	{
		table_opts.no_block_cache = true;
		table_opts.cache_index_and_filter_blocks = false; // MBZ or error w/o block_cache
	}

	// Setup the cache for compressed assets.
	const auto &cache_size_comp(this->descriptor->cache_size_comp);
	if(cache_size_comp != 0)
		table_opts.block_cache_compressed = std::make_shared<database::cache>(this->d, this->stats, this->allocator, this->name, cache_size_comp);

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

//
// snapshot::shapshot
//

ircd::db::database::snapshot::snapshot(database &d)
:s
{
	!d.slave?
		d.d->GetSnapshot():
		nullptr,

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
ircd::log::level
translate(const rocksdb::InfoLogLevel &level)
{
	switch(level)
	{
		// Treat all infomational messages from rocksdb as debug here for now.
		// We can clean them up and make better reports for our users eventually.
		default:
		case rocksdb::InfoLogLevel::DEBUG_LEVEL:     return ircd::log::level::DEBUG;
		case rocksdb::InfoLogLevel::INFO_LEVEL:      return ircd::log::level::DEBUG;

		case rocksdb::InfoLogLevel::WARN_LEVEL:      return ircd::log::level::WARNING;
		case rocksdb::InfoLogLevel::ERROR_LEVEL:     return ircd::log::level::ERROR;
		case rocksdb::InfoLogLevel::FATAL_LEVEL:     return ircd::log::level::CRITICAL;
		case rocksdb::InfoLogLevel::HEADER_LEVEL:    return ircd::log::level::NOTICE;
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
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
#endif __clang__
void
ircd::db::database::logger::Logv(const rocksdb::InfoLogLevel level_,
                                 const char *const fmt,
                                 va_list ap)
noexcept
{
	if(level_ < GetInfoLogLevel())
		return;

	const log::level level
	{
		translate(level_)
	};

	if(level > RB_LOG_LEVEL)
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

	rog(level, "[%s] %s", d->name, str);
}
#ifdef __clang__
#pragma clang diagnostic pop
#endif __clang__
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
	log::critical
	{
		log, "merge: missing merge operator (%s)", e
	};

	return false;
}
catch(const std::exception &e)
{
	log::error
	{
		log, "merge: %s", e
	};

	return false;
}

///////////////////////////////////////////////////////////////////////////////
//
// database::stats (db/database/stats.h) internal
//

namespace ircd::db
{
	static thread_local char database_stats_name_buf[128];
}

//
// stats::stats
//

ircd::db::database::stats::stats(database *const &d,
                                 database::column *const &c)
:d{d}
,c{c}
,get_copied
{
	{ "name", make_name("get.copied")                                   },
	{ "desc", "Number of DB::Get() results violating zero-copy."        },
}
,get_referenced
{
	{ "name", make_name("get.referenced")                               },
	{ "desc", "Number of DB::Get() results adhering to zero-copy."      },
}
,multiget_copied
{
	{ "name", make_name("multiget.copied")                              },
	{ "desc", "Number of DB::MultiGet() results violating zero-copy."   },
}
,multiget_referenced
{
	{ "name", make_name("multiget.referenced")                          },
	{ "desc", "Number of DB::MultiGet() results adhering to zero-copy." },
}
{
	assert(item.size() == ticker.size());
	for(size_t i(0); i < item.size(); ++i)
	{
		const auto &[id, ticker_name]
		{
			rocksdb::TickersNameMap[i]
		};

		assert(id == i);
		new (item.data() + i) ircd::stats::item<uint64_t *>
		{
			std::addressof(ticker[i]), json::members
			{
				{ "name", make_name(ticker_name)                },
				{ "desc", "RocksDB library statistics counter." },
			}
		};
	}
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
ircd::db::database::stats::getAndResetTickerCount(const uint32_t type)
noexcept
{
	const auto ret(getTickerCount(type));
	setTickerCount(type, 0);
	return ret;
}

uint64_t
ircd::db::database::stats::getTickerCount(const uint32_t type)
const noexcept
{
	return ticker.at(type);
}

ircd::string_view
ircd::db::database::stats::make_name(const string_view &ticker_name)
const
{
	assert(this->d);
	return fmt::sprintf
	{
		database_stats_name_buf, "ircd.db.%s.%s.%s",
		db::name(*d),
		c? db::name(*c): "db"s,
		ticker_name,
	};
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

rocksdb::Status
__attribute__((noreturn))
ircd::db::database::stats::passthru::Reset()
noexcept
{
	ircd::terminate
	{
		"Unavailable for passthru"
	};

	__builtin_unreachable();
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

uint64_t
__attribute__((noreturn))
ircd::db::database::stats::passthru::getTickerCount(const uint32_t tickerType)
const noexcept
{
	ircd::terminate
	{
		"Unavailable for passthru"
	};

	__builtin_unreachable();
}

void
__attribute__((noreturn))
ircd::db::database::stats::passthru::setTickerCount(const uint32_t tickerType,
                                                    const uint64_t count)
noexcept
{
	ircd::terminate
	{
		"Unavailable for passthru"
	};

	__builtin_unreachable();
}

void
__attribute__((noreturn))
ircd::db::database::stats::passthru::histogramData(const uint32_t type,
                                                   rocksdb::HistogramData *const data)
const noexcept
{
	ircd::terminate
	{
		"Unavailable for passthru"
	};

	__builtin_unreachable();
}

uint64_t
__attribute__((noreturn))
ircd::db::database::stats::passthru::getAndResetTickerCount(const uint32_t tickerType)
noexcept
{
	ircd::terminate
	{
		"Unavailable for passthru"
	};

	__builtin_unreachable();
}

///////////////////////////////////////////////////////////////////////////////
//
// database::events
//

void
ircd::db::database::events::OnFlushBegin(rocksdb::DB *const db,
                                         const rocksdb::FlushJobInfo &info)
noexcept
{
	log::debug
	{
		log, "[%s] job:%d ctx:%lu flush start '%s' :%s",
		d->name,
		info.job_id,
		info.thread_id,
		info.cf_name,
		reflect(info.flush_reason),
	};

	//assert(info.thread_id == ctx::id(*ctx::current));
}

void
ircd::db::database::events::OnFlushCompleted(rocksdb::DB *const db,
                                             const rocksdb::FlushJobInfo &info)
noexcept
{
	const auto num_deletions
	{
		#if ROCKSDB_MAJOR > 5 \
		|| (ROCKSDB_MAJOR == 5 && ROCKSDB_MINOR > 18) \
		|| (ROCKSDB_MAJOR == 5 && ROCKSDB_MINOR == 18 && ROCKSDB_PATCH >= 3)
			info.table_properties.num_deletions
		#else
			0UL
		#endif
	};

	char pbuf[2][48];
	log::info
	{
		log, "[%s] job:%d ctx:%lu flushed seq[%lu -> %lu] idxs:%lu blks:%lu keys:%lu dels:%lu data[%s] '%s' `%s'",
		d->name,
		info.job_id,
		info.thread_id,
		info.smallest_seqno,
		info.largest_seqno,
		info.table_properties.index_partitions,
		info.table_properties.num_data_blocks,
		info.table_properties.num_entries,
		num_deletions,
		pretty(pbuf[1], iec(info.table_properties.data_size)),
		info.cf_name,
		info.file_path,
	};

	//assert(info.thread_id == ctx::id(*ctx::current));
}

void
ircd::db::database::events::OnCompactionCompleted(rocksdb::DB *const db,
                                                  const rocksdb::CompactionJobInfo &info)
noexcept
{
	using rocksdb::CompactionReason;

	const log::level level
	{
		info.status != rocksdb::Status::OK()?
			log::level::ERROR:
		info.compaction_reason == CompactionReason::kUniversalSizeAmplification?
			log::level::WARNING:
		info.compaction_reason == CompactionReason::kUniversalSortedRunNum?
			log::level::WARNING:
			log::level::INFO
	};

	char prebuf[128];
	const string_view prefix
	{
		fmt::sprintf
		{
			prebuf, "[%s] job:%d ctx:%lu compact",
			d->name,
			info.job_id,
			info.thread_id,
		}
	};

	log::logf
	{
		log, level,
		"%s lev[%d -> %d] files[%zu -> %zu] %s '%s' (%d): %s",
		prefix,
		info.base_input_level,
		info.output_level,
		info.input_files.size(),
		info.output_files.size(),
		reflect(info.compaction_reason),
		info.cf_name,
		int(info.status.code()),
		info.status.getState()?: "OK",
	};

	const bool bytes_same
	{
		info.stats.total_input_bytes == info.stats.total_output_bytes
	};

	char pbuf[8][48];
	size_t i(0);
	if(!bytes_same)
		log::info
		{
			log, "%s key[%zu -> %zu (%zu)] %s -> %s | falloc:%s write:%s rsync:%s fsync:%s total:%s",
			prefix,
			info.stats.num_input_records,
			info.stats.num_output_records,
			info.stats.num_records_replaced,
			pretty(pbuf[i++], iec(info.stats.total_input_bytes)),
			bytes_same? "same": pretty(pbuf[i++], iec(info.stats.total_output_bytes)),
			pretty(pbuf[i++], nanoseconds(info.stats.file_prepare_write_nanos), true),
			pretty(pbuf[i++], nanoseconds(info.stats.file_write_nanos), true),
			pretty(pbuf[i++], nanoseconds(info.stats.file_range_sync_nanos), true),
			pretty(pbuf[i++], nanoseconds(info.stats.file_fsync_nanos), true),
			pretty(pbuf[i++], microseconds(info.stats.elapsed_micros), true),
		};
	assert(i <= 8);

	if(info.stats.num_corrupt_keys > 0)
		log::critical
		{
			log, "[%s] job:%d reported %lu corrupt keys.",
			d->name,
			info.job_id,
			info.stats.num_corrupt_keys
		};

	assert(info.thread_id == ctx::id(*ctx::current));
}

void
ircd::db::database::events::OnTableFileDeleted(const rocksdb::TableFileDeletionInfo &info)
noexcept
{
	const log::level level
	{
		info.status == rocksdb::Status::OK()?
			log::level::DEBUG:
			log::level::ERROR
	};

	log::logf
	{
		log, level,
		"[%s] job:%d table file delete [%s][%s] (%d): %s",
		d->name,
		info.job_id,
		info.db_name,
		lstrip(info.file_path, info.db_name),
		int(info.status.code()),
		info.status.getState()?: "OK",
	};
}

void
ircd::db::database::events::OnTableFileCreated(const rocksdb::TableFileCreationInfo &info)
noexcept
{
	const log::level level
	{
		info.status == rocksdb::Status::OK()?
			log::level::DEBUG:
			log::level::ERROR
	};

	log::logf
	{
		log, level,
		"[%s] job:%d table file closed [%s][%s] size:%s '%s' (%d): %s",
		d->name,
		info.job_id,
		info.db_name,
		lstrip(info.file_path, info.db_name),
		pretty(iec(info.file_size)),
		info.cf_name,
		int(info.status.code()),
		info.status.getState()?: "OK",
	};

	log::debug
	{
		log, "[%s] job:%d head[%s] index[%s] filter[%s] data[%lu %s] keys[%lu %s] vals[%s] %s",
		d->name,
		info.job_id,
		pretty(iec(info.table_properties.top_level_index_size)),
		pretty(iec(info.table_properties.index_size)),
		pretty(iec(info.table_properties.filter_size)),
		info.table_properties.num_data_blocks,
		pretty(iec(info.table_properties.data_size)),
		info.table_properties.num_entries,
		pretty(iec(info.table_properties.raw_key_size)),
		pretty(iec(info.table_properties.raw_value_size)),
		info.table_properties.compression_name
	};
}

void
ircd::db::database::events::OnTableFileCreationStarted(const rocksdb::TableFileCreationBriefInfo &info)
noexcept
{
	log::logf
	{
		log, log::level::DEBUG,
		"[%s] job:%d table file opened [%s][%s] '%s'",
		d->name,
		info.job_id,
		info.db_name,
		lstrip(info.file_path, info.db_name),
		info.cf_name,
	};
}

void
ircd::db::database::events::OnMemTableSealed(const rocksdb::MemTableInfo &info)
noexcept
{
	log::logf
	{
		log, log::level::DEBUG,
		"[%s] [%s] memory table sealed [seq >= %lu first:%lu] entries:%lu deletes:%lu",
		d->name,
		info.cf_name,
		info.earliest_seqno,
		info.first_seqno,
		info.num_entries,
		info.num_deletes,
	};
}

void
ircd::db::database::events::OnColumnFamilyHandleDeletionStarted(rocksdb::ColumnFamilyHandle *const h)
noexcept
{
	log::debug
	{
		log, "[%s] [%s] handle closing @ %p",
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
		log, "[%s] [%s] external file ingested external[%s] internal[%s] sequence:%lu",
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

	const log::level lev
	{
		ignore?
			log::level::DERROR:
		status->severity() == rocksdb::Status::kFatalError?
			log::level::CRITICAL:
		status->severity() == rocksdb::Status::kUnrecoverableError?
			log::level::CRITICAL:
			log::level::ERROR
	};

	log::logf
	{
		log, lev, "[%s] %s", d->name, str
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
	using rocksdb::WriteStallCondition;

	assert(d);
	auto &column
	{
		(*d)[info.cf_name]
	};

	auto prev
	{
		info.condition.prev
	};

	// We seem to be getting these callbacks out of order sometimes. The only
	// way to achieve the proper behavior is to always allow transitions to a
	// normal state, while ignoring any other incorrect transitions.
	const bool changed
	{
		info.condition.cur != WriteStallCondition::kNormal?
			compare_exchange(column.stall, prev, info.condition.cur):
			compare_exchange(column.stall, column.stall, info.condition.cur)
	};

	if(!changed)
		return;

	const auto level
	{
		column.stall == WriteStallCondition::kNormal?
			log::level::INFO:
			log::level::WARNING
	};

	log::logf
	{
		log, level,
		"[%s] [%s] stall condition %s",
		d->name,
		info.cf_name,
		reflect(column.stall),
	};

	assert(column.stall == info.condition.cur);
	//assert(column.stall != WriteStallCondition::kStopped);
}

///////////////////////////////////////////////////////////////////////////////
//
// database::cache (internal)
//

decltype(ircd::db::database::cache::DEFAULT_SHARD_BITS)
ircd::db::database::cache::DEFAULT_SHARD_BITS
(
	std::log2(std::min(size_t(db::request_pool_size), 16UL))
);

decltype(ircd::db::database::cache::DEFAULT_STRICT)
ircd::db::database::cache::DEFAULT_STRICT
{
	false
};

decltype(ircd::db::database::cache::DEFAULT_HI_PRIO)
ircd::db::database::cache::DEFAULT_HI_PRIO
{
	0.25
};

//
// cache::cache
//

ircd::db::database::cache::cache(database *const &d,
                                 std::shared_ptr<struct database::stats> stats,
                                 std::shared_ptr<struct database::allocator> allocator,
                                 std::string name,
                                 const ssize_t &initial_capacity)
#ifdef IRCD_DB_HAS_ALLOCATOR
:rocksdb::Cache{allocator}
,d{d}
#else
:d{d}
#endif
,name{std::move(name)}
,stats{std::move(stats)}
,allocator{std::move(allocator)}
,c{rocksdb::NewLRUCache(rocksdb::LRUCacheOptions
{
	size_t(std::max(initial_capacity, ssize_t(0)))
	,DEFAULT_SHARD_BITS
	,DEFAULT_STRICT
	,DEFAULT_HI_PRIO
	#ifdef IRCD_DB_HAS_ALLOCATOR
	,this->allocator
	#endif
})}
{
	assert(bool(c));
	#ifdef IRCD_DB_HAS_ALLOCATOR
	assert(c->memory_allocator() == this->allocator.get());
	#endif
}

ircd::db::database::cache::~cache()
noexcept
{
}

const char *
ircd::db::database::cache::Name()
const noexcept
{
	return !empty(name)?
		name.c_str():
		c->Name();
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

#ifdef IRCD_DB_HAS_CACHE_GETCHARGE
size_t
ircd::db::database::cache::GetCharge(Handle *const handle)
const noexcept
{
	assert(bool(c));
	return c->GetCharge(handle);
}
#endif

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
	#endif

	static const compactor::callback empty;
	const db::compactor::callback &callback
	{
		type == ValueType::kValue && user.value?
			user.value:

		type == ValueType::kMergeOperand && user.merge?
			user.merge:

		empty
	};

	if(!callback)
		return Decision::kKeep;

	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "[%s]'%s': compaction level:%d key:%zu@%p type:%s old:%zu@%p new:%p skip:%p",
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
	// RocksDB >= 6.0.0 sez this must no longer be false.
	return true;
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
// database::wal_filter
//

decltype(ircd::db::database::wal_filter::debug)
ircd::db::database::wal_filter::debug
{
	{ "name",      "ircd.db.wal.debug" },
	{ "default",   false               },
	{ "persist",   false               },
};

ircd::db::database::wal_filter::wal_filter(database *const &d)
:d{d}
{
}

ircd::db::database::wal_filter::~wal_filter()
noexcept
{
}

void
ircd::db::database::wal_filter::ColumnFamilyLogNumberMap(const log_number_map &log_number,
                                                         const name_id_map &name_id)
noexcept
{
	assert(d);

	this->log_number = log_number;
	this->name_id = name_id;

	log::debug
	{
		log, "[%s] WAL recovery mapping update :log_number:%zu name_id:%zu",
		db::name(*d),
		log_number.size(),
		name_id.size(),
	};
}

rocksdb::WalFilter::WalProcessingOption
ircd::db::database::wal_filter::LogRecordFound(unsigned long long log_nr,
                                               const std::string &name,
                                               const WriteBatch &wb,
                                               WriteBatch *const replace,
                                               bool *const replaced)
noexcept
{
	assert(d && replace && replaced);

	if(debug)
	{
		char buf[128];
		log::logf
		{
			log, log::level::DEBUG,
			"[%s] WAL recovery record log:%lu:%lu '%s' %s",
			db::name(*d),
			d->checkpoint,
			log_nr,
			db::debug(buf, *d, wb),
		};
	}

	*replaced = false;
	return WalProcessingOption::kContinueProcessing;
}

rocksdb::WalFilter::WalProcessingOption
ircd::db::database::wal_filter::LogRecord(const WriteBatch &wb,
                                          WriteBatch *const replace,
                                          bool *const replaced)
const noexcept
{
	return WalProcessingOption::kContinueProcessing;
}

const char *
ircd::db::database::wal_filter::Name()
const noexcept
{
	assert(d);
	return db::name(*d).c_str();
}

///////////////////////////////////////////////////////////////////////////////
//
// database::rate_limiter
//

ircd::db::database::rate_limiter::rate_limiter(database *const &d)
:d{d}
{
}

ircd::db::database::rate_limiter::~rate_limiter()
noexcept
{
}

void
ircd::db::database::rate_limiter::SetBytesPerSecond(int64_t bytes_per_second)
noexcept
{
	log::debug
	{
		log, "[%s] Rate Limiter update rate %zu -> %zu bytes per second",
		db::name(*d),
		this->bytes_per_second,
		bytes_per_second,
	};

	this->bytes_per_second = bytes_per_second;
}

size_t
ircd::db::database::rate_limiter::RequestToken(size_t bytes,
                                               size_t alignment,
                                               IOPriority prio,
                                               Statistics *const stats,
                                               OpType type)
noexcept
{
	#ifdef RB_DEBUG_DB_ENV
	log::debug
	{
		log, "[%s] Rate Limiter request bytes:%zu alignment:%zu prio:%s type:%s",
		db::name(*d),
		bytes,
		alignment,
		reflect(prio),
		type == OpType::kWrite?
			"WRITE"_sv:
		type == OpType::kRead?
			"READ"_sv:
			"????"_sv,
	};
	#endif

	assert(prio <= IOPriority::IO_TOTAL);
	{
		int64_t i(prio == IOPriority::IO_TOTAL? 0: prio); do
		{
			requests[i].bytes += bytes;
			requests[i].count += 1;
		}
		while(++i < prio);
	}

	//assert(stats);
	//stats->recordTick(rocksdb::Tickers::RATE_LIMIT_DELAY_MILLIS, 0);
	//stats->recordTick(rocksdb::Tickers::NUMBER_RATE_LIMITER_DRAINS, 0);
	//stats->recordTick(rocksdb::Tickers::HARD_RATE_LIMIT_DELAY_COUNT, 0);
	//stats->recordTick(rocksdb::Tickers::SOFT_RATE_LIMIT_DELAY_COUNT, 0);

	return bytes;
}

int64_t
ircd::db::database::rate_limiter::GetTotalBytesThrough(const IOPriority prio)
const noexcept
{
	int64_t ret(0);
	int64_t i(prio == IOPriority::IO_TOTAL? 0: prio); do
	{
		ret += requests[i].bytes;
	}
	while(++i < prio);
	return ret;
}

int64_t
ircd::db::database::rate_limiter::GetTotalRequests(const IOPriority prio)
const noexcept
{
	int64_t ret(0);
	int64_t i(prio == IOPriority::IO_TOTAL? 0: prio); do
	{
		ret += requests[i].count;
	}
	while(++i < prio);
	return ret;
}

int64_t
ircd::db::database::rate_limiter::GetSingleBurstBytes()
const noexcept
{
	always_assert(false);
	return bytes_per_second;
}

int64_t
ircd::db::database::rate_limiter::GetBytesPerSecond()
const noexcept
{
	return bytes_per_second;
}

bool
ircd::db::database::rate_limiter::IsRateLimited(OpType op)
noexcept
{
	always_assert(false);
	return false;
}

///////////////////////////////////////////////////////////////////////////////
//
// database::sst
//

void
ircd::db::database::sst::tool(const vector_view<const string_view> &args)
{
	const ctx::uninterruptible::nothrow ui;

	static const size_t arg_max {16};
	static const size_t arg_max_len {256};
	thread_local char arg[arg_max][arg_max_len]
	{
		"./sst_dump"
	};

	size_t i(0);
	char *argv[arg_max] { arg[i++] };
	for(; i < arg_max - 1 && i - 1 < args.size(); ++i)
	{
		strlcpy(arg[i], args.at(i - 1));
		argv[i] = arg[i];
	}
	argv[i++] = nullptr;
	assert(i <= arg_max);

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
	database::column &c(column);
	const database &d(column);
	std::string path
	{
		path_
	};

	if(path.empty())
	{
		const string_view path_parts[]
		{
			fs::base::db, db::name(d), db::name(c)
		};

		path = fs::path_string(path_parts);
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
	for(const auto &c : d.columns) try
	{
		assert(c);
		db::column column{*c};
		for(auto &&info : vector(column))
			this->emplace_back(std::move(info));
	}
	catch(const ctx::interrupted &)
	{
		throw;
	}
	catch(const std::exception &e)
	{
		log::error
		{
			log, "[%s] Failed to query files for '%s' :%s",
			db::name(d),
			db::name(*c),
			e.what(),
		};
	}
}

ircd::db::database::sst::info::vector::vector(const db::column &column)
{
	database::column &c(mutable_cast(column));
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
	auto &d(mutable_cast(d_));
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
	column = std::move(md.column_family_name);
	level = std::move(md.level);
	this->operator=(static_cast<rocksdb::SstFileMetaData &&>(md));
	return *this;
}

ircd::db::database::sst::info &
ircd::db::database::sst::info::operator=(rocksdb::SstFileMetaData &&md)
{
	id = std::move(md.file_number);
	name = std::move(md.name);
	path = std::move(md.db_path);
	size = std::move(md.size);
	min_seq = std::move(md.smallest_seqno);
	max_seq = std::move(md.largest_seqno);
	min_key = std::move(md.smallestkey);
	max_key = std::move(md.largestkey);
	num_reads = std::move(md.num_reads_sampled);
	compacting = std::move(md.being_compacted);

	#if ROCKSDB_MAJOR > 6 \
	|| (ROCKSDB_MAJOR == 6 && ROCKSDB_MINOR > 8) \
	|| (ROCKSDB_MAJOR == 6 && ROCKSDB_MINOR == 8 && ROCKSDB_PATCH >= 1)
		checksum = std::move(md.file_checksum);
		checksum_func = std::move(md.file_checksum_func_name);
	#endif

	#if ROCKSDB_MAJOR > 6 \
	|| (ROCKSDB_MAJOR == 6 && ROCKSDB_MINOR > 7) \
	|| (ROCKSDB_MAJOR == 6 && ROCKSDB_MINOR == 7 && ROCKSDB_PATCH >= 3)
		created = std::move(md.file_creation_time);
	#endif

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
	index_root_size = std::move(tp.top_level_index_size);
	index_data_size = std::move(tp.index_size);
	index_data_size -= index_root_size;
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
	delta_encoding = std::move(tp.index_value_is_delta_encoded);

	blocks_size = keys_size + values_size;
	index_size = index_data_size + index_root_size;
	head_size = index_size + filter_size;
	file_size = head_size + blocks_size;

	meta_size = size > data_size?
		size - data_size:
		0UL;

	compression_pct = file_size > size?
		100 - 100.0L * (size / (long double)file_size):
		0.0;

	index_compression_pct = head_size > meta_size?
		100 - 100.0L * (meta_size / (long double)head_size):
		0.0;

	blocks_compression_pct = blocks_size > data_size?
		100 - 100.0L * (data_size / (long double)blocks_size):
		0.0;

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
	auto &d{mutable_cast(d_)};
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
	auto &d{mutable_cast(d_)};
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
