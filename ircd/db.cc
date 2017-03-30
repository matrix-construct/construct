/*
 * Copyright (C) 2016 Charybdis Development Team
 * Copyright (C) 2016 Jason Volk <jason@zemos.net>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice is present in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <rocksdb/db.h>
#include <rocksdb/cache.h>
#include <rocksdb/comparator.h>
#include <rocksdb/merge_operator.h>
#include <rocksdb/perf_level.h>
#include <rocksdb/listener.h>
#include <rocksdb/statistics.h>
#include <rocksdb/convenience.h>
#include <rocksdb/env.h>

namespace ircd {
namespace db   {

struct log::log log
{
	"db", 'D'            // Database subsystem takes SNOMASK +D
};

struct throw_on_error
{
	throw_on_error(const rocksdb::Status & = rocksdb::Status::OK());
};

const std::string &reflect(const rocksdb::Tickers &);
const std::string &reflect(const rocksdb::Histograms &);
rocksdb::Slice slice(const string_view &);

// Frequently used get options and set options are separate from the string/map system
rocksdb::WriteOptions make_opts(const sopts &);
rocksdb::ReadOptions make_opts(const gopts &, const bool &iterator = false);

enum class pos
{
	FRONT   = -2,    // .front()    | first element
	PREV    = -1,    // std::prev() | previous element
	END     = 0,     // break;      | exit iteration (or past the end)
	NEXT    = 1,     // continue;   | next element
	BACK    = 2,     // .back()     | last element
};

const auto BLOCKING = rocksdb::ReadTier::kReadAllTier;
const auto NON_BLOCKING = rocksdb::ReadTier::kBlockCacheTier;

// This is important to prevent thrashing the iterators which have to reset on iops
const auto DEFAULT_READAHEAD = 4_MiB;

// Validation functors
bool valid(const rocksdb::Iterator &);
bool operator!(const rocksdb::Iterator &);
void valid_or_throw(const rocksdb::Iterator &);
bool valid_equal(const rocksdb::Iterator &, const string_view &);
void valid_equal_or_throw(const rocksdb::Iterator &, const string_view &);

// re-seekers
template<class pos> void seek(column::const_iterator &, const pos &);
template<class pos> void seek(row &, const pos &p);

// Initial seekers
std::unique_ptr<rocksdb::Iterator> seek(column &, const gopts &);
std::unique_ptr<rocksdb::Iterator> seek(column &, const string_view &, const gopts &);
std::vector<row::value_type> seek(database &, const gopts &);

std::pair<string_view, string_view> operator*(const rocksdb::Iterator &);

void append(rocksdb::WriteBatch &, column &, const delta &delta);

std::vector<std::string> column_names(const std::string &path, const rocksdb::DBOptions &);
std::vector<std::string> column_names(const std::string &path, const std::string &options);

struct database::logs
:std::enable_shared_from_this<struct database::logs>
,rocksdb::Logger
{
	database *d;

	// Logger
	void Logv(const rocksdb::InfoLogLevel level, const char *fmt, va_list ap) override;
	void Logv(const char *fmt, va_list ap) override;
	void LogHeader(const char *fmt, va_list ap) override;

	logs(database *const &d)
	:d{d}
	{}
};

struct database::stats
:std::enable_shared_from_this<struct database::stats>
,rocksdb::Statistics
{
	database *d;
	std::array<uint64_t, rocksdb::TICKER_ENUM_MAX> ticker {{0}};
	std::array<rocksdb::HistogramData, rocksdb::HISTOGRAM_ENUM_MAX> histogram;

	uint64_t getTickerCount(const uint32_t tickerType) const override;
	void recordTick(const uint32_t tickerType, const uint64_t count) override;
	void setTickerCount(const uint32_t tickerType, const uint64_t count) override;
	void histogramData(const uint32_t type, rocksdb::HistogramData *) const override;
	void measureTime(const uint32_t histogramType, const uint64_t time) override;
	bool HistEnabledForType(const uint32_t type) const override;

	stats(database *const &d)
	:d{d}
	{}
};

struct database::events
:std::enable_shared_from_this<struct database::events>
,rocksdb::EventListener
{
	database *d;

	void OnFlushCompleted(rocksdb::DB *, const rocksdb::FlushJobInfo &) override;
	void OnCompactionCompleted(rocksdb::DB *, const rocksdb::CompactionJobInfo &) override;
	void OnTableFileDeleted(const rocksdb::TableFileDeletionInfo &) override;
	void OnTableFileCreated(const rocksdb::TableFileCreationInfo &) override;
	void OnTableFileCreationStarted(const rocksdb::TableFileCreationBriefInfo &) override;
	void OnMemTableSealed(const rocksdb::MemTableInfo &) override;
	void OnColumnFamilyHandleDeletionStarted(rocksdb::ColumnFamilyHandle *) override;

	events(database *const &d)
	:d{d}
	{}
};

struct database::mergeop
:std::enable_shared_from_this<struct database::mergeop>
,rocksdb::AssociativeMergeOperator
{
	database *d;
	merge_closure merger;

	bool Merge(const rocksdb::Slice &, const rocksdb::Slice *, const rocksdb::Slice &, std::string *, rocksdb::Logger *) const override;
	const char *Name() const override;

	mergeop(database *const &d, merge_closure merger = nullptr)
	:d{d}
	,merger{merger? std::move(merger) : ircd::db::merge_operator}
	{}
};

struct database::comparator
:rocksdb::Comparator
{
	using Slice = rocksdb::Slice;

	database *d;
	db::comparator user;

	void FindShortestSeparator(std::string *start, const Slice &limit) const override;
	void FindShortSuccessor(std::string *key) const override;
	int Compare(const Slice &a, const Slice &b) const override;
	bool Equal(const Slice &a, const Slice &b) const override;
	const char *Name() const override;

	comparator(database *const &d, db::comparator user)
	:d{d}
	,user{std::move(user)}
	{}
};

struct database::column
:rocksdb::ColumnFamilyDescriptor
{
	database *d;
	std::type_index key_type;
	std::type_index mapped_type;
	comparator cmp;
	rocksdb::ColumnFamilyHandle *handle;

  public:
	operator const rocksdb::ColumnFamilyOptions &();
	operator const rocksdb::ColumnFamilyHandle *() const;
	operator const database &() const;

	operator rocksdb::ColumnFamilyOptions &();
	operator rocksdb::ColumnFamilyHandle *();
	operator database &();

	column(database *const &d, descriptor);
	~column() noexcept;
};

std::map<string_view, database *>
database::dbs
{};

} // namespace db
} // namespace ircd

///////////////////////////////////////////////////////////////////////////////
//
// database
//

void
ircd::db::sync(database &d)
{
	throw_on_error(d.d->SyncWAL());
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

//
// database
//

ircd::db::database::database(const std::string &name,
                             const std::string &optstr,
                             std::initializer_list<descriptor> descriptor)
try
:name
{
	name
}
,path
{
	db::path(this->name)
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
,cache{[this]() -> std::shared_ptr<rocksdb::Cache>
{
	//TODO: XXX
	/*{
		const auto ret(rocksdb::NewLRUCache(lru_cache_size));
		this->opts->row_cache = ret;
		return ret;
	}*/
	return {};
}()}
,d{[this, &descriptor, &optstr]() -> custom_ptr<rocksdb::DB>
{
	rocksdb::DBOptions opts
	{
		options(optstr)
	};

	opts.error_if_exists = false;
	opts.create_if_missing = true;
	opts.create_missing_column_families = true;

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
	opts.wal_recovery_mode = rocksdb::WALRecoveryMode::kAbsoluteConsistency;
	//opts.wal_recovery_mode = rocksdb::WALRecoveryMode::kPointInTimeRecovery;

	// Setup column families
	for(auto &desc : descriptor)
	{
		const auto c(std::make_shared<column>(this, std::move(desc)));
		columns.emplace(c->name, c);
	}

	// Existing columns
	const auto column_names
	{
		db::column_names(path, opts)
	};

	// Specified column descriptors have to describe all existing columns
	for(const auto &name : column_names)
		if(!columns.count(name))
			throw error("Failed to describe existing column '%s'", name);

	// Setup the database closer.
	// We need customization here because of the column family thing.
	const auto deleter([this](rocksdb::DB *const d)
	noexcept
	{
		throw_on_error(d->SyncWAL());                 // blocking
		columns.clear();
		rocksdb::CancelAllBackgroundWork(d, true);    // true = blocking
		throw_on_error(d->PauseBackgroundWork());
		const auto seq(d->GetLatestSequenceNumber());
		delete d;

		log.info("'%s': closed database @ `%s' seq[%zu]",
		         this->name,
		         this->path,
		         seq);
	});

	// Announce attempt before usual point where exceptions are thrown
	log.debug("Opening database \"%s\" @ `%s' columns[%zu]",
	          this->name,
	          path,
	          columns.size());

	// Open DB into ptr
	rocksdb::DB *ptr;
	std::vector<rocksdb::ColumnFamilyHandle *> handles;
	std::vector<rocksdb::ColumnFamilyDescriptor> columns(this->columns.size());
	std::transform(begin(this->columns), end(this->columns), begin(columns), []
	(const auto &pair)
	{
		return static_cast<const rocksdb::ColumnFamilyDescriptor &>(*pair.second);
	});

	//if(has_opt(opts, opt::READ_ONLY))
	//	throw_on_error(rocksdb::DB::OpenForReadOnly(*this->opts, path, columns, &handles, &ptr));
	//else
		throw_on_error(rocksdb::DB::Open(opts, path, columns, &handles, &ptr));

	for(const auto &handle : handles)
		this->columns.at(handle->GetName())->handle = handle;

	// re-establish RAII here
	return { ptr, deleter };
}()}
{
	log.info("'%s': Opened database @ `%s' (handle: %p) columns[%zu] seq[%zu]",
	         name,
	         path,
	         (const void *)this,
	         columns.size(),
	         d->GetLatestSequenceNumber());

	dbs.emplace(string_view{this->name}, this);
}
catch(const std::exception &e)
{
	throw error("Failed to open db '%s': %s", name, e.what());
}

ircd::db::database::~database()
noexcept
{
	log.debug("'%s': closing database @ `%s'", name, path);
	dbs.erase(name);
}

ircd::db::database::column &
ircd::db::database::operator[](const string_view &name)
{
	return *columns.at(name);
}

const ircd::db::database::column &
ircd::db::database::operator[](const string_view &name)
const
{
	return *columns.at(name);
}

///////////////////////////////////////////////////////////////////////////////
//
// database::comparator
//

const char *
ircd::db::database::comparator::Name()
const
{
	assert(!user.name.empty());
	return user.name.c_str();
}

bool
ircd::db::database::comparator::Equal(const Slice &a,
                                      const Slice &b)
const
{
	assert(bool(user.equal));
	const string_view sa{a.data(), a.size()};
	const string_view sb{b.data(), b.size()};
	return user.equal(sa, sb);
}

int
ircd::db::database::comparator::Compare(const Slice &a,
                                        const Slice &b)
const
{
	assert(bool(user.less));
	const string_view sa{a.data(), a.size()};
	const string_view sb{b.data(), b.size()};
	return user.less(sa, sb)? -1:
		   user.less(sb, sa)?  1:
		                       0;
}

void
ircd::db::database::comparator::FindShortSuccessor(std::string *key)
const
{
}

void
ircd::db::database::comparator::FindShortestSeparator(std::string *key,
                                                      const Slice &_limit)
const
{
	const string_view limit{_limit.data(), _limit.size()};
}

namespace ircd {
namespace db   {

struct cmp_string_view
:db::comparator
{
	cmp_string_view()
	:db::comparator
	{
		"string_view"
		,[](const string_view &a, const string_view &b)
		{
			return a < b;
		}
		,[](const string_view &a, const string_view &b)
		{
			return a == b;
		}
	}{}
};

struct cmp_int64_t
:db::comparator
{
	cmp_int64_t()
	:db::comparator
	{
		"int64_t"
		,[](const string_view &sa, const string_view &sb)
		{
			assert(sa.size() == sizeof(int64_t));
			assert(sb.size() == sizeof(int64_t));
			const auto &a(*reinterpret_cast<const int64_t *>(sa.data()));
			const auto &b(*reinterpret_cast<const int64_t *>(sb.data()));
			return a < b;
		}
		,[](const string_view &sa, const string_view &sb)
		{
			assert(sa.size() == sizeof(int64_t));
			assert(sb.size() == sizeof(int64_t));
			const auto &a(*reinterpret_cast<const int64_t *>(sa.data()));
			const auto &b(*reinterpret_cast<const int64_t *>(sb.data()));
			return a == b;
		}
	}{}
};

} // namespace db
} // namespace ircd

///////////////////////////////////////////////////////////////////////////////
//
// database::column
//

ircd::db::database::column::column(database *const &d,
                                   descriptor desc)
:rocksdb::ColumnFamilyDescriptor
{
	std::move(desc.name), database::options(desc.options)
}
,d{d}
,key_type{desc.type.first}
,mapped_type{desc.type.second}
,cmp{d, std::move(desc.cmp)}
,handle{nullptr}
{
	if(!this->cmp.user.less)
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

	this->options.comparator = &this->cmp;

	//if(d->mergeop->merger)
	//	this->options.merge_operator = d->mergeop;

	//log.debug("'%s': Creating new column '%s'", d->name, this->name);
	//throw_on_error(d->d->CreateColumnFamily(this->options, this->name, &this->handle));
}

ircd::db::database::column::~column()
noexcept
{
	if(handle)
		d->d->DestroyColumnFamilyHandle(handle);
}

ircd::db::database::column::operator
database &()
{
	return *d;
}

ircd::db::database::column::operator
rocksdb::ColumnFamilyHandle *()
{
	return handle;
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
	return handle;
}

void
ircd::db::drop(database::column &c)
{
	if(!c.handle)
		return;

	throw_on_error(c.d->d->DropColumnFamily(c.handle));
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

///////////////////////////////////////////////////////////////////////////////
//
// database::snapshot
//

uint64_t
ircd::db::sequence(const database::snapshot &s)
{
	const rocksdb::Snapshot *const rs(s);
	return rs->GetSequenceNumber();
}

ircd::db::database::snapshot::snapshot(database &d)
:d{weak_from(d)}
,s{[this, &d]() -> const rocksdb::Snapshot *
{
	return d.d->GetSnapshot();
}()
,[this](const rocksdb::Snapshot *const s)
{
	if(!s)
		return;

	const auto d(this->d.lock());
	d->d->ReleaseSnapshot(s);
}}
{
}

ircd::db::database::snapshot::~snapshot()
noexcept
{
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
{
	Logv(rocksdb::InfoLogLevel::DEBUG_LEVEL, fmt, ap);
}

void
ircd::db::database::logs::LogHeader(const char *const fmt,
                                    va_list ap)
{
	Logv(rocksdb::InfoLogLevel::DEBUG_LEVEL, fmt, ap);
}

void
ircd::db::database::logs::Logv(const rocksdb::InfoLogLevel level,
                               const char *const fmt,
                               va_list ap)
{
	if(level < GetInfoLogLevel())
		return;

	char buf[1024]; const auto len
	{
		std::vsnprintf(buf, sizeof(buf), fmt, ap)
	};

	const auto str
	{
		// RocksDB adds annoying leading whitespace to attempt to right-justify things and idc
		lstrip(buf, ' ')
	};

	log(translate(level), "'%s': (rdb) %s", d->name, str);
}

const char *
ircd::db::database::mergeop::Name()
const
{
	return "<unnamed>";
}

bool
ircd::db::database::mergeop::Merge(const rocksdb::Slice &_key,
                                   const rocksdb::Slice *const _exist,
                                   const rocksdb::Slice &_update,
                                   std::string *const newval,
                                   rocksdb::Logger *const)
const try
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

bool
ircd::db::database::stats::HistEnabledForType(const uint32_t type)
const
{
	return type < histogram.size();
}

void
ircd::db::database::stats::measureTime(const uint32_t type,
                                       const uint64_t time)
{
}

void
ircd::db::database::stats::histogramData(const uint32_t type,
                                         rocksdb::HistogramData *const data)
const
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
{
	ticker.at(type) += count;
}

void
ircd::db::database::stats::setTickerCount(const uint32_t type,
                                          const uint64_t count)
{
	ticker.at(type) = count;
}

uint64_t
ircd::db::database::stats::getTickerCount(const uint32_t type)
const
{
	return ticker.at(type);
}

void
ircd::db::database::events::OnFlushCompleted(rocksdb::DB *const db,
                                             const rocksdb::FlushJobInfo &info)
{
	log.debug("'%s' @%p: flushed: column[%s] path[%s] tid[%lu] job[%d] writes[slow:%d stop:%d]",
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
{
	log.debug("'%s' @%p: compacted: column[%s] status[%d] tid[%lu] job[%d]",
	          d->name,
	          db,
	          info.cf_name,
	          int(info.status.code()),
	          info.thread_id,
	          info.job_id);
}

void
ircd::db::database::events::OnTableFileDeleted(const rocksdb::TableFileDeletionInfo &info)
{
	log.debug("'%s': table file deleted: db[%s] path[%s] status[%d] job[%d]",
	          d->name,
	          info.db_name,
	          info.file_path,
	          int(info.status.code()),
	          info.job_id);
}

void
ircd::db::database::events::OnTableFileCreated(const rocksdb::TableFileCreationInfo &info)
{
	log.debug("'%s': table file created: db[%s] path[%s] status[%d] job[%d]",
	          d->name,
	          info.db_name,
	          info.file_path,
	          int(info.status.code()),
	          info.job_id);
}

void
ircd::db::database::events::OnTableFileCreationStarted(const rocksdb::TableFileCreationBriefInfo &info)
{
	log.debug("'%s': table file creating: db[%s] column[%s] path[%s] job[%d]",
	          d->name,
	          info.db_name,
	          info.cf_name,
	          info.file_path,
	          info.job_id);
}

void
ircd::db::database::events::OnMemTableSealed(const rocksdb::MemTableInfo &info)
{
	log.debug("'%s': memory table sealed: column[%s] entries[%lu] deletes[%lu]",
	          d->name,
	          info.cf_name,
	          info.num_entries,
	          info.num_deletes);
}

void
ircd::db::database::events::OnColumnFamilyHandleDeletionStarted(rocksdb::ColumnFamilyHandle *const h)
{
	log.debug("'%s': column[%s] handle closing @ %p",
	          d->name,
	          h->GetName(),
	          h);
}

///////////////////////////////////////////////////////////////////////////////
//
// db/row.h
//

ircd::db::row::row(database &d,
                   const string_view &key,
                   gopts opts)
:opts{std::move(opts)}
,its{[this, &d]
{
	return seek(d, this->opts);
}()}
{
	// Piggyback on the snapshot's reference to database.
	// This has to be set here if gopts.snapshot was default initialized.
	if(!this->opts.snapshot)
		this->opts.snapshot.d = weak_from(d);

	if(key.empty())
	{
		seek(*this, pos::FRONT);
		return;
	}

	seek(*this, key);
	const auto end
	{
		std::remove_if(std::begin(its), std::end(its), [&key]
		(auto &pair)
		{
			rocksdb::Iterator &it{*pair.second};
			return !valid_equal(it, key);
		})
	};
	its.erase(end, std::end(its));
}

ircd::db::row::row(row &&o)
noexcept
:opts{std::move(o.opts)}
,its{std::move(o.its)}
{
}

ircd::db::row &
ircd::db::row::operator=(row &&o)
noexcept
{
	its = std::move(o.its);
	opts = std::move(o.opts);

	return *this;
}

ircd::db::row::~row()
noexcept
{
}

ircd::string_view
ircd::db::row::operator[](const string_view &colname)
{
	const auto it(std::find_if(begin(), end(), [&colname]
	(const auto &pair)
	{
		auto &column(pair.first);
		return name(column) == colname;
	}));

	if(it == end())
		return {};

	rocksdb::Iterator &rit
	{
		*it->second
	};

	if(!rit)
		return {};

	const auto pair(*rit);
	return pair.second;
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

size_t
ircd::db::read(column &column,
               const string_view &key,
               uint8_t *const &buf,
               const size_t &max,
               const gopts &gopts)
{
	size_t ret(0);
	const auto copy([&ret, &buf, &max]
	(const string_view &src)
	{
		ret = std::min(src.size(), max);
		memcpy(buf, src.data(), ret);
	});

	column(key, copy, gopts);
	return ret;
}

ircd::string_view
ircd::db::read(column &column,
               const string_view &key,
               char *const &buf,
               const size_t &max,
               const gopts &gopts)
{
	size_t ret(0);
	const auto copy([&ret, &buf, &max]
	(const string_view &src)
	{
		ret = strlcpy(buf, src.data(), std::min(src.size(), max));
	});

	column(key, copy, gopts);
	return { buf, ret };
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
	d.d->GetColumnFamilyMetaData(c.handle, &cfm);
	return cfm.size;
}

size_t
ircd::db::file_count(column &column)
{
	rocksdb::ColumnFamilyMetaData cfm;
	database::column &c(column);
	database &d(c);
	d.d->GetColumnFamilyMetaData(c.handle, &cfm);
	return cfm.file_count;
}

const std::string &
ircd::db::name(const column &column)
{
	const database::column &c(column);
	return name(c);
}

//
// column
//

ircd::db::column::column(database &d,
                         const string_view &column_name)
try
:column
{
	d, *d.columns.at(column_name)
}
{
}
catch(const std::out_of_range &e)
{
	log.error("'%s' failed to open non-existent column '%s'",
	          d.name,
	          column_name);
}

ircd::db::column::column(database::column &c)
:column{*c.d,c}
{
}

ircd::db::column::column(database &d,
                         database::column &c)
:column{shared_from(d), c}
{
}

ircd::db::column::column(std::shared_ptr<database> d,
                         database::column &c)
:d{std::move(d)}
,c{&c}
{
}

void
ircd::db::column::flush(const bool &blocking)
{
	database &d(*this);
	database::column &c(*this);

	rocksdb::FlushOptions opts;
	opts.wait = blocking;

	throw_on_error(d.d->Flush(opts, c));
}

void
ircd::db::del(column &column,
              const string_view &key,
              const sopts &sopts)
{
	database &d(column);
	database::column &c(column);

	auto opts(make_opts(sopts));
	throw_on_error(d.d->Delete(opts, c, slice(key)));
}

void
ircd::db::write(column &column,
                const string_view &key,
                const uint8_t *const &buf,
                const size_t &size,
                const sopts &sopts)
{
	const string_view val
	{
		reinterpret_cast<const char *>(buf), size
	};

	write(column, key, key, sopts);
}

void
ircd::db::write(column &column,
                const string_view &key,
                const string_view &val,
                const sopts &sopts)
{
	database &d(column);
	database::column &c(column);

	auto opts(make_opts(sopts));
	throw_on_error(d.d->Put(opts, c, slice(key), slice(val)));
}

void
ircd::db::append(rocksdb::WriteBatch &batch,
                 column &column,
                 const delta &delta)
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
ircd::db::column::operator()(const op &op,
                             const string_view &key,
                             const string_view &val,
                             const sopts &sopts)
{
	operator()(delta{op, key, val}, sopts);
}

void
ircd::db::column::operator()(const std::initializer_list<delta> &deltas,
                             const sopts &sopts)
{
	rocksdb::WriteBatch batch;
	for(const auto &delta : deltas)
		append(batch, *this, delta);

	database &d(*this);
	auto opts(make_opts(sopts));
	throw_on_error(d.d->Write(opts, &batch));
}

void
ircd::db::column::operator()(const delta &delta,
                             const sopts &sopts)
{
	rocksdb::WriteBatch batch;
	append(batch, *this, delta);

	database &d(*this);
	auto opts(make_opts(sopts));
	throw_on_error(d.d->Write(opts, &batch));
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
	valid_equal_or_throw(*it, key);

	const auto &v(it->value());
	func(string_view{v.data(), v.size()});
}

bool
ircd::db::column::has(const string_view &key,
                      const gopts &gopts)
{
	database &d(*this);
	database::column &c(*this);

	const auto k(slice(key));
	auto opts(make_opts(gopts));

	// Perform queries which are stymied from any sysentry
	opts.read_tier = NON_BLOCKING;

	// Perform a co-RP query to the filtration
	if(!d.d->KeyMayExist(opts, c, k, nullptr, nullptr))
		return false;

	// Perform a query to the cache
	auto status(d.d->Get(opts, c, k, nullptr));
	if(status.IsIncomplete())
	{
		// DB cache miss; next query requires I/O, offload it
		opts.read_tier = BLOCKING;
		ctx::offload([&d, &c, &k, &opts, &status]
		{
			status = d.d->Get(opts, c, k, nullptr);
		});
	}

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
	return cend(gopts);
}

ircd::db::column::const_iterator
ircd::db::column::begin(const gopts &gopts)
{
	return cbegin(gopts);
}

ircd::db::column::const_iterator
ircd::db::column::cend(const gopts &gopts)
{
	return {};
}

ircd::db::column::const_iterator
ircd::db::column::cbegin(const gopts &gopts)
{
	database::column &c(*this);
	const_iterator ret
	{
		c, {}, gopts
	};

	seek(ret, pos::FRONT);
	return std::move(ret);
}

ircd::db::column::const_iterator
ircd::db::column::upper_bound(const string_view &key,
                              const gopts &gopts)
{
	auto it(lower_bound(key, gopts));
	if(it && it.it->key().compare(slice(key)) == 0)
		++it;

	return std::move(it);
}

ircd::db::column::const_iterator
ircd::db::column::find(const string_view &key,
                       const gopts &gopts)
{
	auto it(lower_bound(key, gopts));
	if(!it || it.it->key().compare(slice(key)) != 0)
		return cend(gopts);

	return it;
}

ircd::db::column::const_iterator
ircd::db::column::lower_bound(const string_view &key,
                              const gopts &gopts)
{
	database::column &c(*this);
	const_iterator ret
	{
		c, {}, gopts
	};

	seek(ret, key);
	return std::move(ret);
}

ircd::db::column::const_iterator::const_iterator()
:c{nullptr}
{
}

ircd::db::column::const_iterator::const_iterator(const_iterator &&o)
noexcept
:opts{std::move(o.opts)}
,c{std::move(o.c)}
,it{std::move(o.it)}
,val{std::move(o.val)}
{
}

ircd::db::column::const_iterator &
ircd::db::column::const_iterator::operator=(const_iterator &&o)
noexcept
{
	this->~const_iterator();

	opts = std::move(o.opts);
	c = std::move(o.c);
	it = std::move(o.it);
	val = std::move(o.val);

	return *this;
}

ircd::db::column::const_iterator::const_iterator(database::column &c,
                                                 std::unique_ptr<rocksdb::Iterator> &&it,
                                                 gopts opts)
:opts{std::move(opts)}
,c{&c}
,it{std::move(it)}
{
	//if(!has_opt(this->opts, get::READAHEAD))
	//	this->gopts.readahead_size = DEFAULT_READAHEAD;
}

ircd::db::column::const_iterator::~const_iterator()
noexcept
{
}

ircd::db::column::const_iterator &
ircd::db::column::const_iterator::operator--()
{
	seek(*this, pos::PREV);
	return *this;
}

ircd::db::column::const_iterator &
ircd::db::column::const_iterator::operator++()
{
	seek(*this, pos::NEXT);
	return *this;
}

const ircd::db::column::const_iterator::value_type &
ircd::db::column::const_iterator::operator*()
const
{
	const auto &k(it->key());
	const auto &v(it->value());

	val.first   = { k.data(), k.size() };
	val.second  = { v.data(), v.size() };

	return val;
}

const ircd::db::column::const_iterator::value_type *
ircd::db::column::const_iterator::operator->()
const
{
	return &operator*();
}

bool
ircd::db::column::const_iterator::operator>=(const const_iterator &o)
const
{
	return (*this > o) || (*this == o);
}

bool
ircd::db::column::const_iterator::operator<=(const const_iterator &o)
const
{
	return (*this < o) || (*this == o);
}

bool
ircd::db::column::const_iterator::operator!=(const const_iterator &o)
const
{
	return !(*this == o);
}

bool
ircd::db::column::const_iterator::operator==(const const_iterator &o)
const
{
	if(*this && o)
	{
		const auto &a(it->key());
		const auto &b(o.it->key());
		return a.compare(b) == 0;
	}

	if(!*this && !o)
		return true;

	return false;
}

bool
ircd::db::column::const_iterator::operator>(const const_iterator &o)
const
{
	if(*this && o)
	{
		const auto &a(it->key());
		const auto &b(o.it->key());
		return a.compare(b) == 1;
	}

	if(!*this && o)
		return true;

	if(!*this && !o)
		return false;

	assert(!*this && o);
	return false;
}

bool
ircd::db::column::const_iterator::operator<(const const_iterator &o)
const
{
	if(*this && o)
	{
		const auto &a(it->key());
		const auto &b(o.it->key());
		return a.compare(b) == -1;
	}

	if(!*this && o)
		return false;

	if(!*this && !o)
		return false;

	assert(*this && !o);
	return true;
}

bool
ircd::db::column::const_iterator::operator!()
const
{
	if(!it)
		return true;

	if(!it->Valid())
		return true;

	return false;
}

ircd::db::column::const_iterator::operator bool()
const
{
	return !!*this;
}

///////////////////////////////////////////////////////////////////////////////
//
// Misc
//

std::vector<std::string>
ircd::db::column_names(const std::string &path,
                       const std::string &options)
{
	return column_names(path, database::options(options));
}

std::vector<std::string>
ircd::db::column_names(const std::string &path,
                       const rocksdb::DBOptions &opts)
try
{
	std::vector<std::string> ret;
	throw_on_error(rocksdb::DB::ListColumnFamilies(opts, path, &ret));
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
:options{rocksdb::ColumnFamilyOptions{c.d->d->GetOptions(c.handle)}}
{
}

ircd::db::database::options::options(const rocksdb::DBOptions &opts)
{
	throw_on_error{rocksdb::GetStringFromDBOptions(this, opts)};
}

ircd::db::database::options::options(const rocksdb::ColumnFamilyOptions &opts)
{
	throw_on_error{rocksdb::GetStringFromColumnFamilyOptions(this, opts)};
}

ircd::db::database::options::operator rocksdb::PlainTableOptions()
const
{
	rocksdb::PlainTableOptions ret;
	throw_on_error{rocksdb::GetPlainTableOptionsFromString(ret, *this, &ret)};
	return ret;
}

ircd::db::database::options::operator rocksdb::BlockBasedTableOptions()
const
{
	rocksdb::BlockBasedTableOptions ret;
	throw_on_error{rocksdb::GetBlockBasedTableOptionsFromString(ret, *this, &ret)};
	return ret;
}

ircd::db::database::options::operator rocksdb::ColumnFamilyOptions()
const
{
	rocksdb::ColumnFamilyOptions ret;
	throw_on_error{rocksdb::GetColumnFamilyOptionsFromString(ret, *this, &ret)};
	return ret;
}

ircd::db::database::options::operator rocksdb::DBOptions()
const
{
	rocksdb::DBOptions ret;
	throw_on_error{rocksdb::GetDBOptionsFromString(ret, *this, &ret)};
	return ret;
}

ircd::db::database::options::operator rocksdb::Options()
const
{
	rocksdb::Options ret;
	throw_on_error{rocksdb::GetOptionsFromString(ret, *this, &ret)};
	return ret;
}

ircd::db::database::options::map::map(const options &o)
{
	throw_on_error{rocksdb::StringToMap(o, this)};
}

ircd::db::database::options::map::operator rocksdb::PlainTableOptions()
const
{
	rocksdb::PlainTableOptions ret;
	throw_on_error{rocksdb::GetPlainTableOptionsFromMap(ret, *this, &ret)};
	return ret;
}

ircd::db::database::options::map::operator rocksdb::BlockBasedTableOptions()
const
{
	rocksdb::BlockBasedTableOptions ret;
	throw_on_error{rocksdb::GetBlockBasedTableOptionsFromMap(ret, *this, &ret)};
	return ret;
}

ircd::db::database::options::map::operator rocksdb::ColumnFamilyOptions()
const
{
	rocksdb::ColumnFamilyOptions ret;
	throw_on_error{rocksdb::GetColumnFamilyOptionsFromMap(ret, *this, &ret)};
	return ret;
}

ircd::db::database::options::map::operator rocksdb::DBOptions()
const
{
	rocksdb::DBOptions ret;
	throw_on_error{rocksdb::GetDBOptionsFromMap(ret, *this, &ret)};
	return ret;
}

rocksdb::ReadOptions
ircd::db::make_opts(const gopts &opts,
                    const bool &iterator)
{
	rocksdb::ReadOptions ret;
	ret.snapshot = opts.snapshot;

	if(iterator)
		ret.fill_cache = false;

	for(const auto &opt : opts) switch(opt.first)
	{
		case get::PIN:
			ret.pin_data = true;
			continue;

		case get::CACHE:
			ret.fill_cache = true;
			continue;

		case get::NO_CACHE:
			ret.fill_cache = false;
			continue;

		case get::NO_SNAPSHOT:
			ret.tailing = true;
			continue;

		case get::NO_CHECKSUM:
			ret.verify_checksums = false;
			continue;

		case get::READAHEAD:
			ret.readahead_size = opt.second;
			continue;

		default:
			continue;
	}

	return ret;
}

rocksdb::WriteOptions
ircd::db::make_opts(const sopts &opts)
{
	rocksdb::WriteOptions ret;
	for(const auto &opt : opts) switch(opt.first)
	{
		case set::FSYNC:
			ret.sync = true;
			continue;

		case set::NO_JOURNAL:
			ret.disableWAL = true;
			continue;

		case set::MISSING_COLUMNS:
			ret.ignore_missing_column_families = true;
			continue;

		default:
			continue;
	}

	return ret;
}

namespace ircd {
namespace db   {

void seek(rocksdb::Iterator &, const rocksdb::Slice &);
void seek(rocksdb::Iterator &, const string_view &);
void seek(rocksdb::Iterator &, const pos &);

} // namespace db
} // namespace ircd

std::vector<ircd::db::row::value_type>
ircd::db::seek(database &d,
               const gopts &gopts)
{
	using rocksdb::Iterator;
	using rocksdb::ColumnFamilyHandle;

	const auto opts
	{
		make_opts(gopts, true)
	};

	std::vector<Iterator *> iterators;
	std::vector<database::column *> column(d.columns.size());
	std::transform(begin(d.columns), end(d.columns), begin(column), []
	(const auto &p)
	{
		return p.second.get();
	});

	std::vector<ColumnFamilyHandle *> columns(column.size());
	std::transform(begin(column), end(column), begin(columns), []
	(const auto &ptr)
	{
		return ptr->handle;
	});

	throw_on_error
	{
		d.d->NewIterators(opts, columns, &iterators)
	};

	std::vector<row::value_type> ret(iterators.size());
	for(size_t i(0); i < ret.size(); ++i)
	{
		std::unique_ptr<Iterator> it(iterators.at(i));
		ret[i] = std::make_pair(db::column{*column.at(i)}, std::move(it));
	}

	return ret;
}

std::unique_ptr<rocksdb::Iterator>
ircd::db::seek(column &column,
               const string_view &key,
               const gopts &gopts)
{
	using rocksdb::Iterator;

	database &d(column);
	database::column &c(column);
	auto opts
	{
		make_opts(gopts, true)
	};

	// Perform a query which won't be allowed to do kernel IO
	opts.read_tier = NON_BLOCKING;

	std::unique_ptr<Iterator> it(d.d->NewIterator(opts, c));
	seek(*it, key);

	if(it->status().IsIncomplete())
	{
		// DB cache miss: reset the iterator to blocking mode and offload it
		opts.read_tier = BLOCKING;
		it.reset(d.d->NewIterator(opts, c));
		ctx::offload([&it, &key]
		{
			seek(*it, key);
		});
	}
	// else DB cache hit; no context switch; no thread switch; no kernel I/O; gg

	return std::move(it);
}

template<class pos>
void
ircd::db::seek(row &r,
               const pos &p)
{
	ctx::offload([&r, &p]
	{
		std::for_each(begin(r.its), end(r.its), [&p]
		(const auto &pair)
		{
			rocksdb::Iterator &it(*pair.second);
			seek(it, p);
		});
	});
}

void
ircd::db::seek(row &r,
               const string_view &s)
{
	seek<string_view>(r, s);
}

template<class pos>
void
ircd::db::seek(column::const_iterator &it,
               const pos &p)
{
	const gopts &gopts(it);
	database::column &c(it);
	database &d(*c.d);
	auto opts
	{
		make_opts(gopts, true)
	};

	// Start with a non-blocking query
	if(!it.it || opts.read_tier == BLOCKING)
	{
		opts.read_tier = NON_BLOCKING;
		it.it.reset(d.d->NewIterator(opts, c));
	}

	seek(*it.it, p);
	if(it.it->status().IsIncomplete())
	{
		// DB cache miss: reset the iterator to blocking mode and offload it
		opts.read_tier = BLOCKING;
		it.it.reset(d.d->NewIterator(opts, c));
		ctx::offload([&it, &p]
		{
			seek(*it.it, p);
		});
	}
}

void
ircd::db::seek(column::const_iterator &it,
               const string_view &s)
{
	seek<string_view>(it, s);
}

void
ircd::db::seek(rocksdb::Iterator &it,
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
}

void
ircd::db::seek(rocksdb::Iterator &it,
         const string_view &sv)
{
	seek(it, slice(sv));
}

void
ircd::db::seek(rocksdb::Iterator &it,
         const rocksdb::Slice &sk)
{
	it.Seek(sk);
}

void
ircd::db::valid_equal_or_throw(const rocksdb::Iterator &it,
                               const string_view &sv)
{
	valid_or_throw(it);
	if(it.key().compare(slice(sv)) != 0)
		throw not_found();
}

bool
ircd::db::valid_equal(const rocksdb::Iterator &it,
                      const string_view &sv)
{
	if(!valid(it))
		return false;

	if(it.key().compare(slice(sv)) != 0)
		return false;

	return true;
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
ircd::db::operator!(const rocksdb::Iterator &it)
{
	return !it.Valid();
}

bool
ircd::db::valid(const rocksdb::Iterator &it)
{
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
	const auto &k(it.key());
	const auto &v(it.value());
	return
	{
		{ k.data(), k.size() },
		{ v.data(), v.size() }
	};
}

rocksdb::Slice
ircd::db::slice(const string_view &sv)
{
	return { sv.data(), sv.size() };
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
