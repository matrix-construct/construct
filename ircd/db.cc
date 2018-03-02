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
#include <rocksdb/db.h>
#include <rocksdb/cache.h>
#include <rocksdb/comparator.h>
#include <rocksdb/merge_operator.h>
#include <rocksdb/perf_level.h>
#include <rocksdb/perf_context.h>
#include <rocksdb/listener.h>
#include <rocksdb/statistics.h>
#include <rocksdb/convenience.h>
#include <rocksdb/env.h>
#include <rocksdb/slice_transform.h>

#include <ircd/db/database/comparator.h>
#include <ircd/db/database/prefix_transform.h>
#include <ircd/db/database/mergeop.h>
#include <ircd/db/database/events.h>
#include <ircd/db/database/stats.h>
#include <ircd/db/database/logs.h>
#include <ircd/db/database/column.h>
#include <ircd/db/database/env/env.h>
#include <ircd/db/database/env/writable_file.h>
#include <ircd/db/database/env/sequential_file.h>
#include <ircd/db/database/env/random_access_file.h>
#include <ircd/db/database/env/random_rw_file.h>
#include <ircd/db/database/env/directory.h>
#include <ircd/db/database/env/file_lock.h>

#include "db.h"

decltype(ircd::db::log)
ircd::db::log
{
	// Dedicated logging facility for the database subsystem
	"db", 'D'
};

decltype(ircd::db::rog)
ircd::db::rog
{
	// Dedicated logging facility for rocksdb's log callbacks
	"rdb", 'R'
};

std::map<ircd::string_view, ircd::db::database *>
ircd::db::database::dbs
{};

///////////////////////////////////////////////////////////////////////////////
//
// init
//

namespace ircd::db
{
	static void init_directory();
	static void init_version();
}

static char ircd_db_version[64];
const char *const ircd::db::version(ircd_db_version);

// Renders a version string from the defines included here.
__attribute__((constructor))
static void
ircd::db::init_version()
{
	snprintf(ircd_db_version, sizeof(ircd_db_version), "%d.%d.%d",
	         ROCKSDB_MAJOR,
	         ROCKSDB_MINOR,
	         ROCKSDB_PATCH);
}

static void
ircd::db::init_directory()
try
{
	const auto dbdir(fs::get(fs::DB));
	if(fs::mkdir(dbdir))
		log.notice("Created new database directory at `%s'", dbdir);
	else
		log.info("Using database directory at `%s'", dbdir);
}
catch(const fs::error &e)
{
	log.error("Cannot start database system: %s", e.what());
	if(ircd::debugmode)
		throw;
}

ircd::db::init::init()
{
	init_directory();
}

ircd::db::init::~init()
noexcept
{
}

///////////////////////////////////////////////////////////////////////////////
//
// database
//

void
ircd::db::sync(database &d)
{
	throw_on_error
	{
		d.d->SyncWAL()
	};
}

uint64_t
ircd::db::sequence(const database &d)
{
	return d.d->GetLatestSequenceNumber();
}

template<>
uint64_t
ircd::db::property(database &d,
                   const string_view &name)
{
	uint64_t ret;
	if(!d.d->GetAggregatedIntProperty(slice(name), &ret))
		ret = 0;

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

//
// database
//
namespace ircd::db
{
	database::description default_description;
}

ircd::db::database::database(std::string name,
                             std::string optstr)
:database
{
	std::move(name), std::move(optstr), default_description
}
{
}

ircd::db::database::database(std::string name,
                             std::string optstr,
                             description description)
try
:name
{
	std::move(name)
}
,path
{
	db::path(this->name)
}
,optstr
{
	std::move(optstr)
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
,cache{[this]
() -> std::shared_ptr<rocksdb::Cache>
{
	//TODO: XXX
	const auto lru_cache_size{512_MiB};
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
	const auto opts{make_dbopts(std::string(this->optstr))};
	const auto required{db::column_names(path, opts)};
	std::set<string_view> existing{begin(required), end(required)};

	// The names of the columns extracted from the descriptor set
	std::vector<string_view> ret(descriptors.size());
	std::transform(begin(descriptors), end(descriptors), begin(ret), [&existing]
	(const auto &descriptor) -> string_view
	{
		existing.erase(descriptor.name);
		return descriptor.name;
	});

	for(const auto &remain : existing)
		throw error("Failed to describe existing column '%s' (and %zd others...)",
		            remain,
		            existing.size() - 1);
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
	auto opts(make_dbopts(this->optstr, &read_only, &fsck));

	// Setup sundry
	opts.create_if_missing = true;
	opts.create_missing_column_families = true;
	opts.max_file_opening_threads = 0;
	//opts.use_fsync = true;

	// Setup env
	opts.env = env.get();

	// Setup logging
	logs->SetInfoLogLevel(ircd::debugmode? rocksdb::DEBUG_LEVEL : rocksdb::WARN_LEVEL);
	opts.info_log_level = logs->GetInfoLogLevel();
	opts.info_log = logs;

	// Setup event and statistics callbacks
	opts.listeners.emplace_back(this->events);
	//opts.statistics = this->stats;              // broken?

	// Setup performance metric options
	//rocksdb::SetPerfLevel(rocksdb::PerfLevel::kDisable);

	// Setup journal recovery options
	//opts.wal_recovery_mode = rocksdb::WALRecoveryMode::kTolerateCorruptedTailRecords;
	opts.wal_recovery_mode = rocksdb::WALRecoveryMode::kAbsoluteConsistency;
	//opts.wal_recovery_mode = rocksdb::WALRecoveryMode::kPointInTimeRecovery;

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

	if(fsck && fs::is_dir(path))
	{
		log.info("Checking database @ `%s' columns[%zu]",
		         path,
		         columns.size());

		throw_on_error
		{
			rocksdb::RepairDB(path, opts, columns)
		};

		log.info("Database @ `%s' check complete",
		         path,
		         columns.size());
	}

	// Announce attempt before usual point where exceptions are thrown
	log.debug("Opening database \"%s\" @ `%s' columns[%zu]",
	          this->name,
	          path,
	          columns.size());

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

	try // Assign the column index numbers given by db
	{
		for(const auto &handle : handles)
		{
			this->columns.at(handle->GetID())->handle.reset(handle);
			this->column_index.at(handle->GetName()) = handle->GetID();
		}

		for(size_t i(0); i < this->columns.size(); ++i)
			if(db::id(*this->columns[i]) != i)
				throw error("Columns misaligned: expecting id[%zd] got id[%u] '%s'",
				            i,
				            db::id(*this->columns[i]),
				            db::name(*this->columns[i]));
	}
	catch(const std::exception &e)
	{
		delete ptr;
		throw;
	}

	return custom_ptr<rocksdb::DB>
	{
		ptr, [this](rocksdb::DB *const d) noexcept
		{
			const auto sequence
			{
				d->GetLatestSequenceNumber()
			};

			delete d;
			log.info("'%s': closed database @ `%s' at sequence number %lu.",
			         this->name,
			         this->path,
			         sequence);
		}
	};
}()}
,dbs_it
{
	dbs, dbs.emplace(string_view{this->name}, this).first
}
{
	log.info("'%s': Opened database @ `%s' with %zu columns at sequence number %lu.",
	         this->name,
	         path,
	         columns.size(),
	         d->GetLatestSequenceNumber());
}
catch(const std::exception &e)
{
	throw error("Failed to open db '%s': %s",
	            this->name,
	            e.what());
}

ircd::db::database::~database()
noexcept
{
	rocksdb::CancelAllBackgroundWork(d.get(), true); // true = blocking
	log.debug("'%s': closing database @ `%s'; background_errors: %lu",
	          name,
	          path,
	          property<uint64_t>(*this, rocksdb::DB::Properties::kBackgroundErrors));

	this->columns.clear();
	log.debug("'%s': flushed columns; synchronizing...",
	          name);

	sync(*this);
	log.debug("'%s': synchronized with hardware.",
	          name);
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
		throw schema_error("'%s': column '%s' is not available or specified in schema",
		                   this->name,
		                   name);

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
	throw schema_error("'%s': column id[%u] is not available or specified in schema",
	                   this->name,
	                   id);
}

const ircd::db::database::column &
ircd::db::database::operator[](const string_view &name)
const
{
	const auto it{column_index.find(name)};
	if(unlikely(it == std::end(column_index)))
		throw schema_error("'%s': column '%s' is not available or specified in schema",
		                   this->name,
		                   name);

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
	throw schema_error("'%s': column id[%u] is not available or specified in schema",
	                   this->name,
	                   id);
}

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
	assert(bool(user.equal));
	return user.equal(slice(a), slice(b));
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
ircd::db::database::comparator::FindShortSuccessor(std::string *key)
const noexcept
{
}

void
ircd::db::database::comparator::FindShortestSeparator(std::string *key,
                                                      const Slice &_limit)
const noexcept
{
	const string_view limit{_limit.data(), _limit.size()};
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
ircd::db::flush(database::column &c,
                const bool &blocking)
{
	database &d(*c.d);
	rocksdb::FlushOptions opts;
	opts.wait = blocking;
	log.debug("'%s':'%s' @%lu FLUSH",
	          name(d),
	          name(c),
	          sequence(d));

	throw_on_error
	{
		d.d->Flush(opts, c)
	};
}

void
ircd::db::drop(database::column &c)
{
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
	if(!this->descriptor.cmp.less)
	{
		if(key_type == typeid(string_view))
			this->cmp.user = cmp_string_view{};
		else if(key_type == typeid(int64_t))
			this->cmp.user = cmp_int64_t{};
		else
			throw error("column '%s' key type[%s] requires user supplied comparator",
			            this->name,
			            key_type.name());
	}

	// Set the key comparator
	this->options.comparator = &this->cmp;

	// Set the prefix extractor
	if(this->prefix.user.get && this->prefix.user.has)
		this->options.prefix_extractor = std::shared_ptr<const rocksdb::SliceTransform>
		{
			&this->prefix, [](const rocksdb::SliceTransform *) {}
		};

	//if(d->mergeop->merger)
	//	this->options.merge_operator = d->mergeop;

	//log.debug("'%s': Creating new column '%s'", d->name, this->name);
	//throw_on_error(d->d->CreateColumnFamily(this->options, this->name, &this->handle));

	log.debug("schema '%s' declares column [%s => %s] cmp[%s] prefix[%s]: %s",
	          db::name(*d),
	          demangle(key_type.name()),
	          demangle(mapped_type.name()),
	          this->cmp.Name(),
	          this->options.prefix_extractor? this->prefix.Name() : "none",
	          this->descriptor.name);
}

ircd::db::database::column::~column()
noexcept
{
	if(handle)
		flush(*this, false);
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

const std::string &
ircd::db::name(const database &d)
{
	return d.name;
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
// database::stats
//

void
ircd::db::log_rdb_perf_context(const bool &all)
{
	const bool exclude_zeros(!all);
	log.debug("%s", rocksdb::perf_context.ToString(exclude_zeros));
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
// env
//

ircd::db::database::env::env(database *const &d)
:d{*d}
{
}

ircd::db::database::env::~env()
noexcept
{
}

rocksdb::Status
ircd::db::database::env::NewSequentialFile(const std::string& name,
                                           std::unique_ptr<SequentialFile>* r,
                                           const EnvOptions& options)
noexcept
{
	log.debug("'%s': new sequential file '%s' options:%p",
	          d.name,
	          name,
	          &options);

	std::unique_ptr<SequentialFile> defaults;
	const auto ret
	{
		this->defaults.NewSequentialFile(name, &defaults, options)
	};

	*r = std::make_unique<sequential_file>(&d, name, options, std::move(defaults));
	return ret;
}

rocksdb::Status
ircd::db::database::env::NewRandomAccessFile(const std::string& name,
                                             std::unique_ptr<RandomAccessFile>* r,
                                             const EnvOptions& options)
noexcept
{
	log.debug("'%s': new random access file '%s' options:%p",
	          d.name,
	          name,
	          &options);

	std::unique_ptr<RandomAccessFile> defaults;
	const auto ret
	{
		this->defaults.NewRandomAccessFile(name, &defaults, options)
	};

	*r = std::make_unique<random_access_file>(&d, name, options, std::move(defaults));
	return ret;

}

rocksdb::Status
ircd::db::database::env::NewWritableFile(const std::string& name,
                                         std::unique_ptr<WritableFile>* r,
                                         const EnvOptions& options)
noexcept
{
	log.debug("'%s': new writable file '%s' options:%p",
	          d.name,
	          name,
	          &options);

	std::unique_ptr<WritableFile> defaults;
	const auto ret
	{
		this->defaults.NewWritableFile(name, &defaults, options)
	};

	*r = std::make_unique<writable_file>(&d, name, options, std::move(defaults));
	return ret;
}

rocksdb::Status
ircd::db::database::env::NewRandomRWFile(const std::string& name,
                                         std::unique_ptr<RandomRWFile>* result,
                                         const EnvOptions& options)
noexcept
{
	log.debug("'%s': new random read/write file '%s' options:%p",
	          d.name,
	          name,
	          &options);

	std::unique_ptr<RandomRWFile> defaults;
	const auto ret
	{
		this->defaults.NewRandomRWFile(name, &defaults, options)
	};

	*result = std::make_unique<random_rw_file>(&d, name, options, std::move(defaults));
	return ret;
}

rocksdb::Status
ircd::db::database::env::NewDirectory(const std::string& name,
                                      std::unique_ptr<Directory>* result)
noexcept
{
	log.debug("'%s': new directory '%s'",
	          d.name,
	          name);

	std::unique_ptr<Directory> defaults;
	const auto ret
	{
		this->defaults.NewDirectory(name, &defaults)
	};

	*result = std::make_unique<directory>(&d, name, std::move(defaults));
	return ret;
}

rocksdb::Status
ircd::db::database::env::ReopenWritableFile(const std::string& name,
                                            std::unique_ptr<WritableFile>* result,
                                            const EnvOptions& options)
noexcept
{
	log.debug("'%s': reopen writable file '%s' options:%p",
	          d.name,
	          name,
	          &options);

	return defaults.ReopenWritableFile(name, result, options);
}

rocksdb::Status
ircd::db::database::env::ReuseWritableFile(const std::string& name,
                                           const std::string& old_name,
                                           std::unique_ptr<WritableFile>* r,
                                           const EnvOptions& options)
noexcept
{
	log.debug("'%s': reuse writable file '%s' old '%s' options:%p",
	          d.name,
	          name,
	          old_name,
	          &options);

	return defaults.ReuseWritableFile(name, old_name, r, options);
}

rocksdb::Status
ircd::db::database::env::FileExists(const std::string& f)
noexcept
{
	log.debug("'%s': file exists '%s'",
	          d.name,
	          f);

	return defaults.FileExists(f);
}

rocksdb::Status
ircd::db::database::env::GetChildren(const std::string& dir,
                                     std::vector<std::string>* r)
noexcept
{
	log.debug("'%s': get children of directory '%s'",
	          d.name,
	          dir);

	return defaults.GetChildren(dir, r);
}

rocksdb::Status
ircd::db::database::env::GetChildrenFileAttributes(const std::string& dir,
                                                   std::vector<FileAttributes>* result)
noexcept
{
	log.debug("'%s': get children file attributes of directory '%s'",
	          d.name,
	          dir);

	return defaults.GetChildrenFileAttributes(dir, result);
}

rocksdb::Status
ircd::db::database::env::DeleteFile(const std::string& name)
noexcept
{
	log.debug("'%s': delete file '%s'",
	          d.name,
	          name);

	return defaults.DeleteFile(name);
}

rocksdb::Status
ircd::db::database::env::CreateDir(const std::string& name)
noexcept
{
	log.debug("'%s': create directory '%s'",
	          d.name,
	          d);

	return defaults.CreateDir(name);
}

rocksdb::Status
ircd::db::database::env::CreateDirIfMissing(const std::string& name)
noexcept
{
	log.debug("'%s': create directory if missing '%s'",
	          d.name,
	          name);

	return defaults.CreateDirIfMissing(name);
}

rocksdb::Status
ircd::db::database::env::DeleteDir(const std::string& name)
noexcept
{
	log.debug("'%s': delete directory '%s'",
	          d.name,
	          name);

	return defaults.DeleteDir(name);
}

rocksdb::Status
ircd::db::database::env::GetFileSize(const std::string& name,
                                     uint64_t* s)
noexcept
{
	log.debug("'%s': get file size '%s'",
	          d.name,
	          name);

	return defaults.GetFileSize(name, s);
}

rocksdb::Status
ircd::db::database::env::GetFileModificationTime(const std::string& name,
                                                 uint64_t* file_mtime)
noexcept
{
	log.debug("'%s': get file mtime '%s'",
	          d.name,
	          name);

	return defaults.GetFileModificationTime(name, file_mtime);
}

rocksdb::Status
ircd::db::database::env::RenameFile(const std::string& s,
                                    const std::string& t)
noexcept
{
	log.debug("'%s': rename file '%s' to '%s'",
	          d.name,
	          s,
	          t);

	return defaults.RenameFile(s, t);
}

rocksdb::Status
ircd::db::database::env::LinkFile(const std::string& s,
                                  const std::string& t)
noexcept
{
	log.debug("'%s': link file '%s' to '%s'",
	          d.name,
	          s,
	          t);

	return defaults.LinkFile(s, t);
}

rocksdb::Status
ircd::db::database::env::LockFile(const std::string& name,
                                  FileLock** l)
noexcept
{
	log.debug("'%s': lock file '%s'",
	          d.name,
	          name);

	return defaults.LockFile(name, l);
}

rocksdb::Status
ircd::db::database::env::UnlockFile(FileLock* l)
noexcept
{
	log.debug("'%s': unlock file lock:%p",
	          d.name,
	          l);

	return defaults.UnlockFile(l);
}

void
ircd::db::database::env::Schedule(void (*f)(void* arg),
                                  void* a,
                                  Priority prio,
                                  void* tag,
                                  void (*u)(void* arg))
noexcept
{
	log.debug("'%s': schedule func:%p a:%p tag:%p u:%p prio:%s",
	          d.name,
	          f,
	          a,
	          tag,
	          u,
	          reflect(prio));

	return defaults.Schedule(f, a, prio, tag, u);
}

int
ircd::db::database::env::UnSchedule(void* tag,
                                    Priority pri)
noexcept
{
	log.debug("'%s': unschedule tag:%p prio:%s",
	          d.name,
	          tag,
	          reflect(pri));

	return defaults.UnSchedule(tag, pri);
}

void
ircd::db::database::env::StartThread(void (*f)(void*),
                                     void* a)
noexcept
{
	log.debug("'%s': start thread func:%p a:%p",
	          d.name,
	          f,
	          a);

	return defaults.StartThread(f, a);
}

void
ircd::db::database::env::WaitForJoin()
noexcept
{
	return defaults.WaitForJoin();
}

unsigned int
ircd::db::database::env::GetThreadPoolQueueLen(Priority pri)
const noexcept
{
	log.debug("'%s': get thread pool queue len prio:%s",
	          d.name,
	          reflect(pri));

	return defaults.GetThreadPoolQueueLen(pri);
}

rocksdb::Status
ircd::db::database::env::GetTestDirectory(std::string* path)
noexcept
{
	return defaults.GetTestDirectory(path);
}

rocksdb::Status
ircd::db::database::env::NewLogger(const std::string& name,
                                   std::shared_ptr<Logger>* result)
noexcept
{
	return defaults.NewLogger(name, result);
}

uint64_t
ircd::db::database::env::NowMicros()
noexcept
{
	return defaults.NowMicros();
}

void
ircd::db::database::env::SleepForMicroseconds(int micros)
noexcept
{
	log.debug("'%s': sleep for %d microseconds",
	          d.name,
	          micros);

	defaults.SleepForMicroseconds(micros);
}

rocksdb::Status
ircd::db::database::env::GetHostName(char* name,
                                     uint64_t len)
noexcept
{
	log.debug("'%s': get host name name:%p len:%lu",
	          d.name,
	          name,
	          len);

	return defaults.GetHostName(name, len);
}

rocksdb::Status
ircd::db::database::env::GetCurrentTime(int64_t* unix_time)
noexcept
{
	return defaults.GetCurrentTime(unix_time);
}

rocksdb::Status
ircd::db::database::env::GetAbsolutePath(const std::string& db_path,
                                         std::string* output_path)
noexcept
{
	log.debug("'%s': get absolute path from '%s' ret:%p",
	          d.name,
	          db_path,
	          output_path);

	return defaults.GetAbsolutePath(db_path, output_path);
}

void
ircd::db::database::env::SetBackgroundThreads(int num,
                                              Priority pri)
noexcept
{
	log.debug("'%s': set background threads num:%d prio:%s",
	          d.name,
	          num,
	          reflect(pri));

	return defaults.SetBackgroundThreads(num, pri);
}

void
ircd::db::database::env::IncBackgroundThreadsIfNeeded(int num,
                                                      Priority pri)
noexcept
{
	log.debug("'%s': increase background threads num:%d prio:%s",
	          d.name,
	          num,
	          reflect(pri));

	return defaults.IncBackgroundThreadsIfNeeded(num, pri);
}

void
ircd::db::database::env::LowerThreadPoolIOPriority(Priority pool)
noexcept
{
	log.debug("'%s': lower thread pool priority prio:%s",
	          d.name,
	          reflect(pool));

	defaults.LowerThreadPoolIOPriority(pool);
}

std::string
ircd::db::database::env::TimeToString(uint64_t time)
noexcept
{
	return defaults.TimeToString(time);
}

rocksdb::Status
ircd::db::database::env::GetThreadList(std::vector<ThreadStatus>* thread_list)
noexcept
{
	return defaults.GetThreadList(thread_list);
}

rocksdb::ThreadStatusUpdater*
ircd::db::database::env::GetThreadStatusUpdater()
const noexcept
{
	return defaults.GetThreadStatusUpdater();
}

uint64_t
ircd::db::database::env::GetThreadID()
const noexcept
{
	return defaults.GetThreadID();
}

//
// writable_file
//

ircd::db::database::env::writable_file::writable_file(database *const &d,
                                                      const std::string &name,
                                                      const EnvOptions &opts,
                                                      std::unique_ptr<WritableFile> defaults)
:d{*d}
,defaults{std::move(defaults)}
{
}

ircd::db::database::env::writable_file::~writable_file()
noexcept
{
}

rocksdb::Status
ircd::db::database::env::writable_file::Append(const Slice& s)
noexcept
{
/*
	log.debug("'%s': wfile:%p append:%p bytes:%zu",
	          d.name,
	          this,
	          data(s),
	          size(s));
*/

	return defaults->Append(s);
}

rocksdb::Status
ircd::db::database::env::writable_file::PositionedAppend(const Slice& s,
                                                         uint64_t offset)
noexcept
{
	log.debug("'%s': wfile:%p append:%p bytes:%zu offset:%lu",
	          d.name,
	          this,
	          data(s),
	          size(s),
	          offset);

	return defaults->PositionedAppend(s, offset);
}

rocksdb::Status
ircd::db::database::env::writable_file::Truncate(uint64_t size)
noexcept
{
	log.debug("'%s': wfile:%p truncate to %lu bytes",
	          d.name,
	          this,
	          size);

	return defaults->Truncate(size);
}

rocksdb::Status
ircd::db::database::env::writable_file::Close()
noexcept
{
/*
	log.debug("'%s': wfile:%p close",
	          d.name,
	          this);
*/
	return defaults->Close();
}

rocksdb::Status
ircd::db::database::env::writable_file::Flush()
noexcept
{
/*
	log.debug("'%s': wfile:%p flush",
	          d.name,
	          this);
*/
	return defaults->Flush();
}

rocksdb::Status
ircd::db::database::env::writable_file::Sync()
noexcept
{
	log.debug("'%s': wfile:%p sync",
	          d.name,
	          this);

	return defaults->Sync();
}

rocksdb::Status
ircd::db::database::env::writable_file::Fsync()
noexcept
{
	log.debug("'%s': wfile:%p fsync",
	          d.name,
	          this);

	return defaults->Fsync();
}

bool
ircd::db::database::env::writable_file::IsSyncThreadSafe()
const noexcept
{
	return defaults->IsSyncThreadSafe();
}

void
ircd::db::database::env::writable_file::SetIOPriority(Env::IOPriority prio)
noexcept
{
	log.debug("'%s': wfile:%p set IO prio to %s",
	          d.name,
	          this,
	          reflect(prio));

	defaults->SetIOPriority(prio);
}

rocksdb::Env::IOPriority
ircd::db::database::env::writable_file::GetIOPriority()
noexcept
{
	return defaults->GetIOPriority();
}

uint64_t
ircd::db::database::env::writable_file::GetFileSize()
noexcept
{
	return defaults->GetFileSize();
}

void
ircd::db::database::env::writable_file::GetPreallocationStatus(size_t* block_size,
                                                               size_t* last_allocated_block)
noexcept
{
	log.debug("'%s': wfile:%p get preallocation block_size:%p last_block:%p",
	          d.name,
	          this,
	          block_size,
	          last_allocated_block);

	defaults->GetPreallocationStatus(block_size, last_allocated_block);
}

size_t
ircd::db::database::env::writable_file::GetUniqueId(char* id,
                                                    size_t max_size)
const noexcept
{
	log.debug("'%s': wfile:%p get unique id:%p max_size:%zu",
	          d.name,
	          this,
	          id,
	          max_size);

	return defaults->GetUniqueId(id, max_size);
}

rocksdb::Status
ircd::db::database::env::writable_file::InvalidateCache(size_t offset,
                                                        size_t length)
noexcept
{
	log.debug("'%s': wfile:%p invalidate cache offset:%zu length:%zu",
	          d.name,
	          this,
	          offset,
	          length);

	return defaults->InvalidateCache(offset, length);
}

void
ircd::db::database::env::writable_file::SetPreallocationBlockSize(size_t size)
noexcept
{
	log.debug("'%s': wfile:%p set preallocation block size:%zu",
	          d.name,
	          this,
	          size);

	defaults->SetPreallocationBlockSize(size);
}

void
ircd::db::database::env::writable_file::PrepareWrite(size_t offset,
                                                     size_t length)
noexcept
{
/*
	log.debug("'%s': wfile:%p prepare write offset:%zu length:%zu",
	          d.name,
	          this,
	          offset,
	          length);
*/
	defaults->PrepareWrite(offset, length);
}

rocksdb::Status
ircd::db::database::env::writable_file::Allocate(uint64_t offset,
                                                 uint64_t length)
noexcept
{
	log.debug("'%s': wfile:%p allocate offset:%lu length:%lu",
	          d.name,
	          this,
	          offset,
	          length);

	return defaults->Allocate(offset, length);
}

rocksdb::Status
ircd::db::database::env::writable_file::RangeSync(uint64_t offset,
                                                  uint64_t length)
noexcept
{
	log.debug("'%s': wfile:%p range sync offset:%lu length:%lu",
	          d.name,
	          this,
	          offset,
	          length);

	return defaults->RangeSync(offset, length);
}

//
// sequential_file
//

ircd::db::database::env::sequential_file::sequential_file(database *const &d,
                                                          const std::string &name,
                                                          const EnvOptions &opts,
                                                          std::unique_ptr<SequentialFile> defaults)
:d{*d}
,defaults{std::move(defaults)}
{
}

ircd::db::database::env::sequential_file::~sequential_file()
noexcept
{
}

rocksdb::Status
ircd::db::database::env::sequential_file::Read(size_t length,
                                               Slice *result,
                                               char *scratch)
noexcept
{
/*
	log.debug("'%s': seqfile:%p read:%p length:%zu scratch:%p",
	          d.name,
	          this,
	          result,
	          length,
	          scratch);
*/
	return defaults->Read(length, result, scratch);
}

rocksdb::Status
ircd::db::database::env::sequential_file::PositionedRead(uint64_t offset,
                                                         size_t length,
                                                         Slice *result,
                                                         char *scratch)
noexcept
{
/*
	log.debug("'%s': seqfile:%p read:%p length:%zu offset:%zu scratch:%p",
	          d.name,
	          this,
	          result,
	          length,
	          offset,
	          scratch);
*/
	return defaults->PositionedRead(offset, length, result, scratch);
}

rocksdb::Status
ircd::db::database::env::sequential_file::Skip(uint64_t size)
noexcept
{
	log.debug("'%s': seqfile:%p skip:%zu",
	          d.name,
	          this,
	          size);

	return defaults->Skip(size);
}

rocksdb::Status
ircd::db::database::env::sequential_file::InvalidateCache(size_t offset,
                                                          size_t length)
noexcept
{
	log.debug("'%s': seqfile:%p invalidate cache offset:%zu length:%zu",
	          d.name,
	          this,
	          offset,
	          length);

	return defaults->InvalidateCache(offset, length);
}

bool
ircd::db::database::env::sequential_file::use_direct_io()
const noexcept
{
	return defaults->use_direct_io();
}

size_t
ircd::db::database::env::sequential_file::GetRequiredBufferAlignment()
const noexcept
{
	return defaults->GetRequiredBufferAlignment();
}

//
// random_access_file
//

ircd::db::database::env::random_access_file::random_access_file(database *const &d,
                                                                const std::string &name,
                                                                const EnvOptions &opts,
                                                               std::unique_ptr<RandomAccessFile> defaults)
:d{*d}
,defaults{std::move(defaults)}
{
}

ircd::db::database::env::random_access_file::~random_access_file()
noexcept
{
}

rocksdb::Status
ircd::db::database::env::random_access_file::Prefetch(uint64_t offset,
                                                      size_t length)
noexcept
{
/*
	log.debug("'%s': rfile:%p prefetch offset:%zu length:%zu",
	          d.name,
	          this,
	          offset,
	          length);
*/
	return defaults->Prefetch(offset, length);
}

rocksdb::Status
ircd::db::database::env::random_access_file::Read(uint64_t offset,
                                                  size_t length,
                                                  Slice *result,
                                                  char *scratch)
const noexcept
{
/*
	log.debug("'%s': rfile:%p read:%p offset:%zu length:%zu scratch:%p",
	          d.name,
	          this,
	          result,
	          offset,
	          length,
	          scratch);
*/
	return defaults->Read(offset, length, result, scratch);
}

rocksdb::Status
ircd::db::database::env::random_access_file::InvalidateCache(size_t offset,
                                                             size_t length)
noexcept
{
	log.debug("'%s': rfile:%p invalidate cache offset:%zu length:%zu",
	          d.name,
	          this,
	          offset,
	          length);

	return defaults->InvalidateCache(offset, length);
}

size_t
ircd::db::database::env::random_access_file::GetUniqueId(char* id,
                                                         size_t max_size)
const noexcept
{
	log.debug("'%s': rfile:%p get unique id:%p max_size:%zu",
	          d.name,
	          this,
	          id,
	          max_size);

	return defaults->GetUniqueId(id, max_size);
}

void
ircd::db::database::env::random_access_file::Hint(AccessPattern pattern)
noexcept
{
/*
	log.debug("'%s': rfile:%p hint %s",
	          d.name,
	          this,
	          reflect(pattern));
*/
	return defaults->Hint(pattern);
}

bool
ircd::db::database::env::random_access_file::use_direct_io()
const noexcept
{
	return defaults->use_direct_io();
}

size_t
ircd::db::database::env::random_access_file::GetRequiredBufferAlignment()
const noexcept
{
	return defaults->GetRequiredBufferAlignment();
}

//
// random_rw_file
//

ircd::db::database::env::random_rw_file::random_rw_file(database *const &d,
                                                        const std::string &name,
                                                        const EnvOptions &opts,
                                                        std::unique_ptr<RandomRWFile> defaults)
:d{*d}
,defaults{std::move(defaults)}
{
}

ircd::db::database::env::random_rw_file::~random_rw_file()
noexcept
{
}

rocksdb::Status
ircd::db::database::env::random_rw_file::Close()
noexcept
{
	log.debug("'%s': rwfile:%p closec",
	          d.name,
	          this);

	return defaults->Close();
}

rocksdb::Status
ircd::db::database::env::random_rw_file::Fsync()
noexcept
{
	log.debug("'%s': rwfile:%p fsync",
	          d.name,
	          this);

	return defaults->Fsync();
}

rocksdb::Status
ircd::db::database::env::random_rw_file::Sync()
noexcept
{
	log.debug("'%s': rwfile:%p sync",
	          d.name,
	          this);

	return defaults->Sync();
}

rocksdb::Status
ircd::db::database::env::random_rw_file::Flush()
noexcept
{
	log.debug("'%s': rwfile:%p flush",
	          d.name,
	          this);

	return defaults->Flush();
}

rocksdb::Status
ircd::db::database::env::random_rw_file::Read(uint64_t offset,
                                              size_t length,
                                              Slice *result,
                                              char *scratch)
const noexcept
{
	log.debug("'%s': rwfile:%p read:%p offset:%zu length:%zu scratch:%p",
	          d.name,
	          this,
	          result,
	          offset,
	          length,
	          scratch);

	return defaults->Read(offset, length, result, scratch);
}

rocksdb::Status
ircd::db::database::env::random_rw_file::Write(uint64_t offset,
                                               const Slice &slice)
noexcept
{
	log.debug("'%s': rwfile:%p write:%p offset:%zu length:%zu",
	          d.name,
	          this,
	          data(slice),
	          offset,
	          size(slice));

	return defaults->Write(offset, slice);
}

bool
ircd::db::database::env::random_rw_file::use_direct_io()
const noexcept
{
	return defaults->use_direct_io();
}

size_t
ircd::db::database::env::random_rw_file::GetRequiredBufferAlignment()
const noexcept
{
	return defaults->GetRequiredBufferAlignment();
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
	log.debug("'%s': directory:%p fsync",
	          d.name,
	          this);

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

///////////////////////////////////////////////////////////////////////////////
//
// db/txn.h
//

struct ircd::db::txn::handler
:rocksdb::WriteBatch::Handler
{
	using Status = rocksdb::Status;
	using Slice = rocksdb::Slice;

	const database &d;
	const std::function<bool (const delta &)> &cb;
	bool _continue {true};

	Status callback(const delta &) noexcept;
	Status callback(const uint32_t &, const op &, const Slice &a, const Slice &b) noexcept;

	bool Continue() noexcept override;
	Status MarkRollback(const Slice &xid) noexcept override;
	Status MarkCommit(const Slice &xid) noexcept override;
	Status MarkEndPrepare(const Slice &xid) noexcept override;
	Status MarkBeginPrepare() noexcept override;

	Status MergeCF(const uint32_t cfid, const Slice &, const Slice &) noexcept override;
	Status SingleDeleteCF(const uint32_t cfid, const Slice &) noexcept override;
	Status DeleteRangeCF(const uint32_t cfid, const Slice &, const Slice &) noexcept override;
	Status DeleteCF(const uint32_t cfid, const Slice &) noexcept override;
	Status PutCF(const uint32_t cfid, const Slice &, const Slice &) noexcept override;

	handler(const database &d,
	        const std::function<bool (const delta &)> &cb)
	:d{d}
	,cb{cb}
	{}
};

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

/// Iterate the txn using the "until protocol"
/// reminder: the closure remains-true-until-the-end; false to break;
/// returns true if the end reached; false if broken early
bool
ircd::db::until(const txn &t,
                const std::function<bool (const delta &)> &closure)
{
	const database &d(t);
	const rocksdb::WriteBatch &wb{t};
	txn::handler h{d, closure};
	wb.Iterate(&h);
	return h._continue;
}

/// Iterate the txn using the "test protocol"
/// reminder: the closure returns true to break, false to continue;
/// returns true if broken early, false if the end reached.
bool
ircd::db::test(const txn &t,
               const std::function<bool (const delta &)> &closure)
{
	return !until(t, [&closure]
	(const delta &delta)
	{
		return !closure(delta);
	});
}

///
/// handler
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
	log::critical("txn::handler: cfid[%u]: %s", cfid, e.what());
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
:txn{d, {}}
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
	return test(*this, [&op, &col]
	(const auto &delta)
	{
		return std::get<delta.OP>(delta) == op &&
		       std::get<delta.COL>(delta) == col;
	});
}

bool
ircd::db::txn::has(const op &op,
                   const string_view &col,
                   const string_view &key)
const
{
	return test(*this, [&op, &col, &key]
	(const auto &delta)
	{
		return std::get<delta.OP>(delta) == op &&
		       std::get<delta.COL>(delta) == col &&
		       std::get<delta.KEY>(delta) == key;
	});
}

ircd::db::delta
ircd::db::txn::at(const op &op,
                  const string_view &col)
const
{
	const auto ret(get(op, col));
	if(unlikely(!std::get<ret.KEY>(ret)))
		throw not_found("db::txn::at(%s, %s): no matching delta in transaction",
		                reflect(op),
		                col);
	return ret;
}

ircd::db::delta
ircd::db::txn::get(const op &op,
                   const string_view &col)
const
{
	delta ret;
	test(*this, [&ret, &op, &col]
	(const delta &delta)
	{
		if(std::get<delta.OP>(delta) == op &&
		   std::get<delta.COL>(delta) == col)
		{
			ret = delta;
			return true;
		}
		else return false;
	});

	return ret;
}

ircd::string_view
ircd::db::txn::at(const op &op,
                  const string_view &col,
                  const string_view &key)
const
{
	const auto ret(get(op, col, key));
	if(unlikely(!ret))
		throw not_found("db::txn::at(%s, %s, %s): no matching delta in transaction",
		                reflect(op),
		                col,
		                key);
	return ret;
}

ircd::string_view
ircd::db::txn::get(const op &op,
                   const string_view &col,
                   const string_view &key)
const
{
	string_view ret;
	test(*this, [&ret, &op, &col, &key]
	(const delta &delta)
	{
		if(std::get<delta.OP>(delta) == op &&
		   std::get<delta.COL>(delta) == col &&
		   std::get<delta.KEY>(delta) == key)
		{
			ret = std::get<delta.VAL>(delta);
			return true;
		}
		else return false;
	});

	return ret;
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
               const pos &p,
               gopts opts)
{
	opts |= index::applied_opts;
	return seek(static_cast<column::const_iterator_base &>(it), p, opts);
}
template bool ircd::db::seek<ircd::db::pos>(index::const_iterator_base &, const pos &, gopts);
template bool ircd::db::seek<ircd::string_view>(index::const_iterator_base &, const string_view &, gopts);

ircd::db::index::const_iterator
ircd::db::index::begin(const string_view &key,
                       gopts opts)
{
	const_iterator ret
	{
		c, {}, opts.snapshot
	};

	seek(ret, key, opts);
	return ret;
}

ircd::db::index::const_iterator
ircd::db::index::end(const string_view &key,
                     gopts opts)
{
	const_iterator ret
	{
		c, {}, opts.snapshot
	};

	if(seek(ret, key, opts))
		seek(ret, pos::END, opts);

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
		c, {}, opts.snapshot
	};

	if(seek(ret, key, opts))
	{
		while(seek(ret, pos::NEXT, opts));
		seek(ret, pos::PREV, opts);
	}

	return ret;
}

ircd::db::index::const_reverse_iterator
ircd::db::index::rend(const string_view &key,
                      gopts opts)
{
	const_reverse_iterator ret
	{
		c, {}, opts.snapshot
	};

	if(seek(ret, key, opts))
		seek(ret, pos::END, opts);

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
,it{!index.empty()? seek(this->c, index, opts) : std::unique_ptr<rocksdb::Iterator>{}}
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

	std::unique_ptr<rocksdb::TransactionLogIterator> tit;
	throw_on_error(d.d->GetUpdatesSince(0, &tit));
	while(tit && tit->Valid())
	{
		auto batchres(tit->GetBatch());
		//std::cout << "seq: " << batchres.sequence;
		if(batchres.writeBatchPtr)
		{
			auto &batch(*batchres.writeBatchPtr);
			//std::cout << " count " << batch.Count() << " ds: " << batch.GetDataSize() << " " << batch.Data() << std::endl;
		}

		tit->Next();
	}

	database::column &c(this->c);
	return seek(c, index, opts, this->it);
}

ircd::string_view
ircd::db::cell::exchange(const string_view &desired)
{
	const auto ret(val());
	(*this) = desired;
	return ret;
}

bool
ircd::db::cell::compare_exchange(string_view &expected,
                                 const string_view &desired)
{
	const auto existing(val());
	if(expected.size() != existing.size() ||
	   memcmp(expected.data(), existing.data(), expected.size()) != 0)
	{
		expected = existing;
		return false;
	}

	expected = existing;
	(*this) = desired;
	return true;
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

template<class pos>
size_t
ircd::db::seek(row &r,
               const pos &p)
{
	if(r.empty())
		return 0;

	const column &c(r[0]);
	const database &d(c);
//	const ircd::timer timer;
	const auto ret
	{
		std::count_if(begin(r), end(r), [&p]
		(auto &cell)
		{
			return seek(cell, p);
		})
	};
/*
	log.debug("'%s' %lu:%lu '%s' row SEEK %zu of %zu in %ld$us",
	          name(d),
	          sequence(d),
	          sequence(r[0]),
	          name(c),
	          ret,
	          r.size(),
	          timer.at<microseconds>().count());
*/
	return ret;
}
template size_t ircd::db::seek<ircd::db::pos>(row &, const pos &);
template size_t ircd::db::seek<ircd::string_view>(row &, const string_view &);

//
// row
//

ircd::db::row::row(database &d,
                   const string_view &key,
                   const vector_view<string_view> &colnames,
                   const vector_view<cell> &buf,
                   gopts opts)
:vector_view<cell>{buf}
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

	//TODO: allocator
	std::vector<database::column *> colptr
	{
		colnames.empty()? d.columns.size() : colnames.size()
	};

	if(colnames.empty())
		std::transform(begin(d.columns), end(d.columns), begin(colptr), [&colnames]
		(const auto &p)
		{
			return p.get();
		});
	else
		std::transform(begin(colnames), end(colnames), begin(colptr), [&d]
		(const auto &name)
		{
			return &d[name];
		});

	//TODO: allocator
	std::vector<ColumnFamilyHandle *> handles(colptr.size());
	std::transform(begin(colptr), end(colptr), begin(handles), []
	(database::column *const &ptr)
	{
		return ptr->handle.get();
	});

	//TODO: does this block?
	std::vector<Iterator *> iterators;
	throw_on_error
	{
		d.d->NewIterators(options, handles, &iterators)
	};

	for(size_t i(0); i < this->size(); ++i)
	{
		std::unique_ptr<Iterator> it(iterators.at(i));
		(*this)[i] = cell { *colptr.at(i), key, std::move(it), opts };
	}
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
		throw schema_error("column '%s' not specified in the descriptor schema", column);

	return *it;
}

const ircd::db::cell &
ircd::db::row::operator[](const string_view &column)
const
{
	const auto it(find(column));
	if(unlikely(it == end()))
		throw schema_error("column '%s' not specified in the descriptor schema", column);

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
	const auto copy([&ret]
	(const string_view &src)
	{
		ret.assign(begin(src), end(src));
	});

	column(key, copy, gopts);
	return ret;
}

ircd::string_view
ircd::db::read(column &column,
               const string_view &key,
               const mutable_buffer &buf,
               const gopts &gopts)
{
	size_t len(0);
	const auto copy([&len, &buf]
	(const string_view &src)
	{
		len = std::min(size(src), size(buf));
		memcpy(data(buf), data(src), len);
	});

	column(key, copy, gopts);
	return { data(buf), len };
}

template<>
std::string
ircd::db::property(column &column,
                   const string_view &name)
{
	std::string ret;
	database &d(column);
	database::column &c(column);
	d.d->GetProperty(c, slice(name), &ret);
	return ret;
}

template<>
uint64_t
ircd::db::property(column &column,
                   const string_view &name)
{
	uint64_t ret;
	database &d(column);
	database::column &c(column);
	if(!d.d->GetIntProperty(c, slice(name), &ret))
		ret = 0;

	return ret;
}

size_t
ircd::db::bytes(column &column)
{
	rocksdb::ColumnFamilyMetaData cfm;
	database::column &c(column);
	database &d(c);
	assert(bool(c.handle));
	d.d->GetColumnFamilyMetaData(c.handle.get(), &cfm);
	return cfm.size;
}

size_t
ircd::db::file_count(column &column)
{
	rocksdb::ColumnFamilyMetaData cfm;
	database::column &c(column);
	database &d(c);
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
ircd::db::flush(column &column,
                const bool &blocking)
{
	database::column &c(column);
	flush(c, blocking);
}

void
ircd::db::del(column &column,
              const string_view &key,
              const sopts &sopts)
{
	database &d(column);
	database::column &c(column);
	log.debug("'%s' %lu '%s' DELETE key(%zu B)",
	          name(d),
	          sequence(d),
	          name(c),
	          key.size());

	auto opts(make_opts(sopts));
	throw_on_error
	{
		d.d->Delete(opts, c, slice(key))
	};
}

void
ircd::db::write(column &column,
                const string_view &key,
                const mutable_buffer &buf,
                const sopts &sopts)
{
	const string_view val
	{
		reinterpret_cast<const char *>(data(buf)), size(buf)
	};

	write(column, key, val, sopts);
}

void
ircd::db::write(column &column,
                const string_view &key,
                const string_view &val,
                const sopts &sopts)
{
	database &d(column);
	database::column &c(column);
	log.debug("'%s' %lu '%s' PUT key(%zu B) val(%zu B)",
	          name(d),
	          sequence(d),
	          name(c),
	          key.size(),
	          val.size());

	auto opts(make_opts(sopts));
	throw_on_error
	{
		d.d->Put(opts, c, slice(key), slice(val))
	};
}

///
/// TODO: As soon as rocksdb regressed from allowing a null value argument
/// to the query, we are forced to watch rocksdb allocate and copy the value
/// into our string `discard` for no reason at all. If this regression remains
/// it may be more efficient to use a regular iterator seek like everything
/// else. Otherwise if they fix this, the discard string should become a
/// nullptr instead.
bool
ircd::db::has(column &column,
              const string_view &key,
              const gopts &gopts)
{
	database &d(column);
	database::column &c(column);

	const auto k(slice(key));
	auto opts(make_opts(gopts));

	// Perform queries which are stymied from any sysentry
	opts.read_tier = NON_BLOCKING;

	// Perform a co-RP query to the filtration
	bool value{false};
	static thread_local std::string discard;
	const bool may_exist{d.d->KeyMayExist(opts, c, k, &discard, &value)};

	if(!may_exist)
		return false;

	// Perform a query to the cache
	auto status(d.d->Get(opts, c, k, &discard));
	if(status.IsIncomplete())
	{
		// DB cache miss; next query requires I/O, offload it
		opts.read_tier = BLOCKING;
		ctx::offload([&d, &c, &k, &opts, &status]
		{
			status = d.d->Get(opts, c, k, &discard);
		});
	}

	log.debug("'%s' %lu:%lu '%s' HAS key(%zu B) %s%s",
	          name(d),
	          sequence(d),
	          sequence(opts.snapshot),
	          name(c),
	          key.size(),
	          status.ok()? "YES"s : "NO"s,
	          opts.read_tier == BLOCKING? " CACHE MISS"s : ""s);

	// Finally the result
	switch(status.code())
	{
		using rocksdb::Status;

		case Status::kOk:          return true;
		case Status::kNotFound:    return false;
		default:
			throw_on_error(status);
			__builtin_unreachable();
	}
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

///////////////////////////////////////////////////////////////////////////////
//
// column::const_iterator
//

namespace ircd {
namespace db   {

} // namespace db
} // namespace ircd

ircd::db::column::const_iterator
ircd::db::column::end(const gopts &gopts)
{
	const_iterator ret
	{
		c, {}, gopts.snapshot
	};

	seek(ret, pos::END, gopts);
	return ret;
}

ircd::db::column::const_iterator
ircd::db::column::begin(const gopts &gopts)
{
	const_iterator ret
	{
		c, {}, gopts.snapshot
	};

	seek(ret, pos::FRONT, gopts);
	return ret;
}

ircd::db::column::const_reverse_iterator
ircd::db::column::rend(const gopts &gopts)
{
	const_reverse_iterator ret
	{
		c, {}, gopts.snapshot
	};

	seek(ret, pos::END, gopts);
	return ret;
}

ircd::db::column::const_reverse_iterator
ircd::db::column::rbegin(const gopts &gopts)
{
	const_reverse_iterator ret
	{
		c, {}, gopts.snapshot
	};

	seek(ret, pos::BACK, gopts);
	return ret;
}

ircd::db::column::const_iterator
ircd::db::column::upper_bound(const string_view &key,
                              const gopts &gopts)
{
	auto it(lower_bound(key, gopts));
	if(it && it.it->key().compare(slice(key)) == 0)
		++it;

	return it;
}

ircd::db::column::const_iterator
ircd::db::column::find(const string_view &key,
                       const gopts &gopts)
{
	auto it(lower_bound(key, gopts));
	if(!it || it.it->key().compare(slice(key)) != 0)
		return end(gopts);

	return it;
}

ircd::db::column::const_iterator
ircd::db::column::lower_bound(const string_view &key,
                              const gopts &gopts)
{
	const_iterator ret
	{
		c, {}, gopts.snapshot
	};

	seek(ret, key, gopts);
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
,ss{std::move(o.ss)}
,it{std::move(o.it)}
,val{std::move(o.val)}
{
}

ircd::db::column::const_iterator_base &
ircd::db::column::const_iterator_base::operator=(const_iterator_base &&o)
noexcept
{
	c = std::move(o.c);
	ss = std::move(o.ss);
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
                                                 database::snapshot ss)
:c{c}
,ss{std::move(ss)}
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
               const pos &p,
               const gopts &opts)
{
	database::column &c(it);
	const auto ropts
	{
		make_opts(opts)
	};

	return seek(c, p, ropts, it.it);
}
template bool ircd::db::seek<ircd::db::pos>(column::const_iterator_base &, const pos &, const gopts &);
template bool ircd::db::seek<ircd::string_view>(column::const_iterator_base &, const string_view &, const gopts &);

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
	log.debug("'%s' %lu COMMIT %s",
	          d.name,
	          sequence(d),
	          debug(batch));

	throw_on_error
	{
		d.d->Write(opts, &batch)
	};
}

std::string
ircd::db::debug(const rocksdb::WriteBatch &batch)
{
	std::string ret;
	ret.resize(511, char());

	const auto size
	{
		snprintf(const_cast<char *>(ret.data()), ret.size() + 1,
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
		         batch.HasRollback()? " ROLLBACK" : "")
	};

	ret.resize(size);
	return ret;
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
	static std::unique_ptr<rocksdb::Iterator> _seek_offload(database::column &c, const rocksdb::ReadOptions &opts, const std::function<void (rocksdb::Iterator &)> &closure);
	bool _seek(database::column &, const pos &, const rocksdb::ReadOptions &, std::unique_ptr<rocksdb::Iterator> &it);
	bool _seek(database::column &, const string_view &, const rocksdb::ReadOptions &, std::unique_ptr<rocksdb::Iterator> &it);
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
	assert(opts.read_tier == NON_BLOCKING);

	if(!it)
	{
		database &d(*c.d);
		rocksdb::ColumnFamilyHandle *const &cf(c);
		it.reset(d.d->NewIterator(opts, cf));
	}

	return _seek(c, p, opts, it);
}

//
// Seek with offload-safety in case of blocking IO.
//
// The options for an iterator cannot be changed after the iterator is created.
// This slightly complicates our toggling between blocking and non-blocking queries.
//
bool
ircd::db::_seek(database::column &c,
                const string_view &p,
                const rocksdb::ReadOptions &opts,
                std::unique_ptr<rocksdb::Iterator> &it)
{
	database &d(*c.d);
	const ircd::timer timer;

	// Start with a non-blocking query.
	_seek_(*it, p);

	// Branch for query being fulfilled from cache
	if(!it->status().IsIncomplete())
	{
/*		log.debug("'%s' %lu:%lu SEEK %s %s in %ld$us '%s'",
		          name(d),
		          sequence(d),
		          sequence(opts.snapshot),
		          it->status().ToString(),
		          valid(*it)? "VALID" : "INVALID",
		          timer.at<microseconds>().count(),
		          name(c));
*/
		return valid(*it);
	}

	const auto blocking_it
	{
		_seek_offload(c, opts, [&p](rocksdb::Iterator &blocking_it)
		{
			_seek_(blocking_it, p);
		})
	};

	// When the blocking iterator comes back invalid the result is propagated
	if(!valid(*blocking_it))
	{
		it.reset(rocksdb::NewErrorIterator(blocking_it->status()));
		log.debug("'%s' %lu:%lu SEEK %s INVALID CACHE MISS in %ld$us '%s'",
		          name(d),
		          sequence(d),
		          sequence(opts.snapshot),
		          it->status().ToString(),
		          timer.at<microseconds>().count(),
		          name(c));

		return false;
	}

	it.reset(nullptr);
/*
	log.debug("'%s' %lu:%lu SEEK %s VALID CACHE MISS in %ld$us '%s'",
	          name(d),
	          sequence(d),
	          sequence(opts.snapshot),
	          blocking_it->status().ToString(),
	          timer.at<microseconds>().count(),
	          name(c));
*/

	return seek(c, slice(blocking_it->key()), opts, it);
}

//
// Seek with offload-safety in case of blocking IO.
//
// The options for an iterator cannot be changed after the iterator is created.
// This slightly complicates our toggling between blocking and non-blocking queries.
//
bool
ircd::db::_seek(database::column &c,
                const pos &p,
                const rocksdb::ReadOptions &opts,
                std::unique_ptr<rocksdb::Iterator> &it)
{
	database &d(*c.d);
	const ircd::timer timer;
	const bool valid_it
	{
		valid(*it)
	};

	// Start with a non-blocking query.
	_seek_(*it, p);

	// Branch for query being fulfilled from cache
	if(!it->status().IsIncomplete())
	{
/*
		log.debug("'%s' %lu:%lu SEEK[%s] %s %s -> %s in %ld$us '%s'",
		          name(d),
		          sequence(d),
		          sequence(opts.snapshot),
		          reflect(p),
		          it->status().ToString(),
		          valid_it? "VALID" : "INVALID",
		          valid(*it)? "VALID" : "INVALID",
		          timer.at<microseconds>().count(),
		          name(c));
*/
		return valid(*it);
	}

	const auto it_key
	{
		valid_it? slice(it->key()) : string_view{}
	};

	const auto blocking_it
	{
		_seek_offload(c, opts, [&valid_it, &it_key, &p]
		(rocksdb::Iterator &blocking_it)
		{
			if(valid_it)
				_seek_(blocking_it, it_key);

			_seek_(blocking_it, p);
		})
	};

	// When the blocking iterator comes back invalid the result is propagated
	if(!valid(*blocking_it))
	{
		it.reset(rocksdb::NewErrorIterator(blocking_it->status()));
		log.debug("'%s' %lu:%lu SEEK[%s] %s %s -> %s|INVALID CACHE MISS in %ld$us '%s'",
		          name(d),
		          sequence(d),
		          sequence(opts.snapshot),
		          reflect(p),
		          it->status().ToString(),
		          valid_it? "VALID" : "INVALID",
		          valid(*it)? "VALID" : "INVALID",
		          timer.at<microseconds>().count(),
		          name(c));

		return false;
	}

	it.reset(nullptr);
	log.debug("'%s' %lu:%lu SEEK[%s] %s %s -> VALID CACHE MISS in %ld$us '%s'",
	          name(d),
	          sequence(d),
	          sequence(opts.snapshot),
	          reflect(p),
	          blocking_it->status().ToString(),
	          valid_it? "VALID" : "INVALID",
	          timer.at<microseconds>().count(),
	          name(c));

	return seek(c, slice(blocking_it->key()), opts, it);
}

/// DB cache miss: create a blocking iterator and offload it.
std::unique_ptr<rocksdb::Iterator>
ircd::db::_seek_offload(database::column &c,
                        const rocksdb::ReadOptions &opts,
                        const std::function<void (rocksdb::Iterator &)> &closure)
{
	database &d(*c.d);
	rocksdb::ColumnFamilyHandle *const &cf(c);
	rocksdb::ReadOptions blocking_opts(opts);
	blocking_opts.fill_cache = true;
	blocking_opts.read_tier = BLOCKING;
	std::unique_ptr<rocksdb::Iterator> blocking_it
	{
		d.d->NewIterator(blocking_opts, cf)
	};

	ctx::offload([&closure, &blocking_it]
	{
		closure(*blocking_it);
	});

	return blocking_it;
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

template<class... args>
rocksdb::DBOptions
ircd::db::make_dbopts(const std::string &optstr,
                      args&&... a)
{
	std::string _optstr(optstr);
	return make_dbopts(_optstr, std::forward<args>(a)...);
}

rocksdb::DBOptions
ircd::db::make_dbopts(std::string &optstr,
                      bool *const read_only,
                      bool *const fsck)
{
	// RocksDB doesn't parse a read_only option, so we allow that to be added
	// to open the database as read_only and then remove that from the string.
	if(read_only)
		*read_only = optstr_find_and_remove(optstr, "read_only=true;"s);

	// We also allow the user to specify fsck=true to run a repair operation on
	// the db. This may be expensive to do by default every startup.
	if(fsck)
		*fsck = optstr_find_and_remove(optstr, "fsck=true;"s);

	// Generate RocksDB options from string
	rocksdb::DBOptions opts
	{
		database::options(optstr)
	};

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
	ret.read_tier = NON_BLOCKING;
	ret.iterate_upper_bound = opts.upper_bound;

	// slice* for exclusive upper bound. when prefixes are used this value must
	// have the same prefix because ordering is not guaranteed between prefixes
	//ret.iterate_upper_bound = nullptr;

	ret += opts;
	return ret;
}

rocksdb::ReadOptions &
ircd::db::operator+=(rocksdb::ReadOptions &ret,
                     const gopts &opts)
{
	if(opts.snapshot && !test(opts, get::NO_SNAPSHOT))
		ret.snapshot = opts.snapshot;

	ret.pin_data = test(opts, get::PIN);
	ret.fill_cache |= test(opts, get::CACHE);
	ret.fill_cache &= !test(opts, get::NO_CACHE);
	ret.tailing = test(opts, get::NO_SNAPSHOT);
	ret.verify_checksums = !test(opts, get::NO_CHECKSUM);
	ret.prefix_same_as_start = test(opts, get::PREFIX);
	ret.total_order_seek = test(opts, get::ORDERED);
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
	if(!valid_eq(it, sv))
	{
		throw_on_error(it.status());
		throw not_found();
	}
}

void
ircd::db::valid_or_throw(const rocksdb::Iterator &it)
{
	if(!valid(it))
	{
		throw_on_error(it.status());
		throw not_found();
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
		default:
			throw error("code[%d] %s", s.code(), s.ToString());
	}
}

std::vector<std::string>
ircd::db::available()
{
	const auto prefix(fs::get(fs::DB));
	const auto dirs(fs::ls(prefix));
	return dirs;
}

std::string
ircd::db::path(const std::string &name)
{
	const auto prefix(fs::get(fs::DB));
	return fs::make_path({prefix, name});
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
		case rocksdb::Env::Priority::LOW:     return "LOW"_sv;
		case rocksdb::Env::Priority::HIGH:    return "HIGH"_sv;
		case rocksdb::Env::Priority::TOTAL:   return "TOTAL"_sv;
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
		case rocksdb::Env::IOPriority::IO_TOTAL:   return "IO_TOTAL"_sv;
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
