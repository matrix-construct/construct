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

const std::string &reflect(const rocksdb::Tickers &);
const std::string &reflect(const rocksdb::Histograms &);

void throw_on_error(const rocksdb::Status &);
bool valid(const rocksdb::Iterator &);
void valid_or_throw(const rocksdb::Iterator &);
void valid_equal_or_throw(const rocksdb::Iterator &, const rocksdb::Slice &);

const auto BLOCKING = rocksdb::ReadTier::kReadAllTier;
const auto NON_BLOCKING = rocksdb::ReadTier::kBlockCacheTier;
enum class pos
{
	FRONT   = -2,    // .front()    | first element
	PREV    = -1,    // std::prev() | previous element
	END     = 0,     // break;      | exit iteration (or past the end)
	NEXT    = 1,     // continue;   | next element
	BACK    = 2,     // .back()     | last element
};
void seek(rocksdb::Iterator &, const pos &);
void seek(rocksdb::Iterator &, const rocksdb::Slice &);
template<class pos> void seek(rocksdb::DB &, rocksdb::ColumnFamilyHandle *const &, const pos &, rocksdb::ReadOptions &, std::unique_ptr<rocksdb::Iterator> &);
std::unique_ptr<rocksdb::Iterator> seek(rocksdb::DB &, rocksdb::ColumnFamilyHandle *const &, const rocksdb::Slice &, rocksdb::ReadOptions &);

// This is important to prevent thrashing the iterators which have to reset on iops
const auto DEFAULT_READAHEAD = 4_MiB;

rocksdb::WriteOptions make_opts(const sopts &);
rocksdb::ReadOptions make_opts(const gopts &, const bool &iterator = false);
rocksdb::Options make_opts(const opts &);

std::vector<std::string> column_names(const std::string &path, const rocksdb::DBOptions &);
std::vector<std::string> column_names(const std::string &path, const opts &);

struct meta
:std::enable_shared_from_this<meta>
{
	static std::map<std::string, meta *> dbs; // open databases

	struct logs;
	struct stats;
	struct events;
	struct mergeop;

	std::string name;
	std::string path;
	rocksdb::Options opts;
	std::shared_ptr<struct logs> logs;
	std::shared_ptr<struct stats> stats;
	std::shared_ptr<struct events> events;
	std::shared_ptr<struct mergeop> mergeop;
	std::shared_ptr<rocksdb::Cache> cache;
	std::vector<rocksdb::ColumnFamilyDescriptor> columns;
	std::vector<rocksdb::ColumnFamilyHandle *> handles;
	custom_ptr<rocksdb::DB> d;

	meta(const std::string &name,
         const db::opts &opts,
         merge_function mf);

	~meta() noexcept;
};

struct meta::logs
:std::enable_shared_from_this<struct meta::logs>
,rocksdb::Logger
{
	struct meta *meta;

	// Logger
	void Logv(const rocksdb::InfoLogLevel level, const char *fmt, va_list ap) override;
	void Logv(const char *fmt, va_list ap) override;
	void LogHeader(const char *fmt, va_list ap) override;

	logs(struct meta *const &meta)
	:meta{meta}
	{}
};

struct meta::stats
:std::enable_shared_from_this<struct meta::stats>
,rocksdb::Statistics
{
	struct meta *meta;
	std::array<uint64_t, rocksdb::TICKER_ENUM_MAX> ticker {{0}};
	std::array<rocksdb::HistogramData, rocksdb::HISTOGRAM_ENUM_MAX> histogram;

	uint64_t getTickerCount(const uint32_t tickerType) const override;
	void recordTick(const uint32_t tickerType, const uint64_t count) override;
	void setTickerCount(const uint32_t tickerType, const uint64_t count) override;
	void histogramData(const uint32_t type, rocksdb::HistogramData *) const override;
	void measureTime(const uint32_t histogramType, const uint64_t time) override;
	bool HistEnabledForType(const uint32_t type) const override;

	stats(struct meta *const &meta)
	:meta{meta}
	{}
};

struct meta::events
:std::enable_shared_from_this<struct meta::events>
,rocksdb::EventListener
{
	struct meta *meta;

	void OnFlushCompleted(rocksdb::DB *, const rocksdb::FlushJobInfo &) override;
	void OnCompactionCompleted(rocksdb::DB *, const rocksdb::CompactionJobInfo &) override;
	void OnTableFileDeleted(const rocksdb::TableFileDeletionInfo &) override;
	void OnTableFileCreated(const rocksdb::TableFileCreationInfo &) override;
	void OnTableFileCreationStarted(const rocksdb::TableFileCreationBriefInfo &) override;
	void OnMemTableSealed(const rocksdb::MemTableInfo &) override;
	void OnColumnFamilyHandleDeletionStarted(rocksdb::ColumnFamilyHandle *) override;

	events(struct meta *const &meta)
	:meta{meta}
	{}
};

struct meta::mergeop
:std::enable_shared_from_this<struct meta::mergeop>
,rocksdb::AssociativeMergeOperator
{
	struct meta *meta;
	merge_function merger;

	bool Merge(const rocksdb::Slice &, const rocksdb::Slice *, const rocksdb::Slice &, std::string *, rocksdb::Logger *) const override;
	const char *Name() const override;

	mergeop(struct meta *const &meta, merge_function merger = nullptr)
	:meta{meta}
	,merger{std::move(merger)}
	{}
};

std::map<std::string, meta *>
meta::dbs
{};

} // namespace db
} // namespace ircd

using namespace ircd;

db::init::init()
{
}

db::init::~init()
noexcept
{
}

///////////////////////////////////////////////////////////////////////////////
//
// Meta
//

db::meta::meta(const std::string &name,
               const db::opts &opts,
               merge_function merger)
try
:name{name}
,path{db::path(name)}
,opts{make_opts(opts)}
,logs{std::make_shared<struct logs>(this)}
,stats{std::make_shared<struct stats>(this)}
,events{std::make_shared<struct events>(this)}
,mergeop{std::make_shared<struct mergeop>(this, std::move(merger))}
,cache{[this, &opts]() -> std::shared_ptr<rocksdb::Cache>
{
	const auto lru_cache_size
	{
		opt_val(opts, opt::LRU_CACHE)
	};

	if(lru_cache_size > 0)
	{
		const auto ret(rocksdb::NewLRUCache(lru_cache_size));
		this->opts.row_cache = ret;
		return ret;
	}
	else return {};
}()}
,d{[this, &opts, &name]() -> custom_ptr<rocksdb::DB>
{
	// Setup logging
	logs->SetInfoLogLevel(ircd::debugmode? rocksdb::DEBUG_LEVEL : rocksdb::WARN_LEVEL);
	this->opts.info_log_level = logs->GetInfoLogLevel();
	this->opts.info_log = logs;

	// Setup event and statistics callbacks
	this->opts.listeners.emplace_back(this->events);
	//this->opts.statistics = this->stats;              // broken?

	// Setup performance metric options
	//rocksdb::SetPerfLevel(rocksdb::PerfLevel::kDisable);

	// Setup journal recovery options
	this->opts.wal_recovery_mode = rocksdb::WALRecoveryMode::kAbsoluteConsistency;
	//this->opts.wal_recovery_mode = rocksdb::WALRecoveryMode::kPointInTimeRecovery;

	 // Setup user operators
	if(mergeop->merger)
		this->opts.merge_operator = this->mergeop;

	// Setup column families
	const auto column_names{db::column_names(path, this->opts)};
	this->opts.create_missing_column_families = true;
	for(const auto &name : column_names)
		columns.emplace_back(name, this->opts);

	// Setup the database closer.
	// We need customization here because of the column family thing.
	const auto deleter([this](rocksdb::DB *const d)
	noexcept
	{
		for(auto it(std::rbegin(this->handles)); it != std::rend(this->handles); ++it)
			d->DestroyColumnFamilyHandle(*it);

		throw_on_error(d->SyncWAL());                 // blocking
		rocksdb::CancelAllBackgroundWork(d, true);    // true = blocking
		const auto seq(d->GetLatestSequenceNumber());
		delete d;

		log.info("'%s': closed database @ `%s' seq[%zu]",
		         this->name,
		         this->path,
		         seq);
	});

	// Announce attempt before usual point where exceptions are thrown
	log.debug("Opening database \"%s\" @ `%s' columns: %zu",
	          name,
	          path,
	          columns.size());

	// Open DB into ptr
	rocksdb::DB *ptr;
	if(has_opt(opts, opt::READ_ONLY))
		throw_on_error(rocksdb::DB::OpenForReadOnly(this->opts, path, columns, &handles, &ptr));
	else
		throw_on_error(rocksdb::DB::Open(this->opts, path, columns, &handles, &ptr));

	// re-establish RAII here
	return { ptr, deleter };
}()}
{
	log.info("'%s': Opened database @ `%s' (handle: %p) columns: %zu",
	         name,
	         path,
	         (const void *)this,
	         handles.size());

	dbs.emplace(name, this);
}
catch(const invalid_argument &e)
{
	const bool no_create(has_opt(opts, opt::NO_CREATE));
	const bool no_existing(has_opt(opts, opt::NO_EXISTING));
	const auto helpstr
	{
		no_create?   " (The database is missing and will not be created)":
		no_existing? " (The database already exists but must be fresh)":
		             ""
	};

	throw error("Failed to open db '%s': %s%s", name, e, helpstr);
}
catch(const std::exception &e)
{
	throw error("Failed to open db '%s': %s", name, e);
}

ircd::db::meta::~meta()
noexcept
{
	log.debug("'%s': closing database @ `%s'", name, path);
	dbs.erase(name);
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
ircd::db::meta::logs::Logv(const char *const fmt,
                           va_list ap)
{
	Logv(rocksdb::InfoLogLevel::DEBUG_LEVEL, fmt, ap);
}

void
ircd::db::meta::logs::LogHeader(const char *const fmt,
                                va_list ap)
{
	Logv(rocksdb::InfoLogLevel::DEBUG_LEVEL, fmt, ap);
}

void
ircd::db::meta::logs::Logv(const rocksdb::InfoLogLevel level,
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

	log(translate(level), "'%s': (rdb) %s", meta->name, str);
}

const char *
ircd::db::meta::mergeop::Name()
const
{
	return "<unnamed>";
}

bool
ircd::db::meta::mergeop::Merge(const rocksdb::Slice &_key,
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
ircd::db::meta::stats::HistEnabledForType(const uint32_t type)
const
{
	return type < histogram.size();
}

void
ircd::db::meta::stats::measureTime(const uint32_t type,
                                   const uint64_t time)
{
}

void
ircd::db::meta::stats::histogramData(const uint32_t type,
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
ircd::db::meta::stats::recordTick(const uint32_t type,
                                   const uint64_t count)
{
	ticker.at(type) += count;
}

void
ircd::db::meta::stats::setTickerCount(const uint32_t type,
                                      const uint64_t count)
{
	ticker.at(type) = count;
}

uint64_t
ircd::db::meta::stats::getTickerCount(const uint32_t type)
const
{
	return ticker.at(type);
}

void
ircd::db::meta::events::OnFlushCompleted(rocksdb::DB *const db,
                                         const rocksdb::FlushJobInfo &info)
{
	log.debug("'%s' @%p: flushed: column[%s] path[%s] tid[%lu] job[%d] writes[slow:%d stop:%d]",
	          meta->name,
	          db,
	          info.cf_name,
	          info.file_path,
	          info.thread_id,
	          info.job_id,
	          info.triggered_writes_slowdown,
	          info.triggered_writes_stop);
}

void
ircd::db::meta::events::OnCompactionCompleted(rocksdb::DB *const db,
                                               const rocksdb::CompactionJobInfo &info)
{
	log.debug("'%s' @%p: compacted: column[%s] status[%d] tid[%lu] job[%d]",
	          meta->name,
	          db,
	          info.cf_name,
	          int(info.status.code()),
	          info.thread_id,
	          info.job_id);
}

void
ircd::db::meta::events::OnTableFileDeleted(const rocksdb::TableFileDeletionInfo &info)
{
	log.debug("'%s': table file deleted: db[%s] path[%s] status[%d] job[%d]",
	          meta->name,
	          info.db_name,
	          info.file_path,
	          int(info.status.code()),
	          info.job_id);
}

void
ircd::db::meta::events::OnTableFileCreated(const rocksdb::TableFileCreationInfo &info)
{
	log.debug("'%s': table file created: db[%s] path[%s] status[%d] job[%d]",
	          meta->name,
	          info.db_name,
	          info.file_path,
	          int(info.status.code()),
	          info.job_id);
}

void
ircd::db::meta::events::OnTableFileCreationStarted(const rocksdb::TableFileCreationBriefInfo &info)
{
	log.debug("'%s': table file creating: db[%s] column[%s] path[%s] job[%d]",
	          meta->name,
	          info.db_name,
	          info.cf_name,
	          info.file_path,
	          info.job_id);
}

void
ircd::db::meta::events::OnMemTableSealed(const rocksdb::MemTableInfo &info)
{
	log.debug("'%s': memory table sealed: column[%s] entries[%lu] deletes[%lu]",
	          meta->name,
	          info.cf_name,
	          info.num_entries,
	          info.num_deletes);
}

void
ircd::db::meta::events::OnColumnFamilyHandleDeletionStarted(rocksdb::ColumnFamilyHandle *const h)
{
	log.debug("'%s': column family handle deletion started: %p",
	          meta->name,
	          h);
}

///////////////////////////////////////////////////////////////////////////////
//
// Handle
//

db::handle::handle()
:meta{}
,h{nullptr}
{
}

db::handle::handle(const std::string &name,
                   const std::string &column,
                   const opts &opts,
                   merge_function mf)
try
:meta{[&name, &column, &opts, &mf]
{
	const auto it(meta::dbs.find(name));
	if(it != std::end(meta::dbs))
	{
		const auto &meta(it->second);
		return meta->shared_from_this();
	}

	return std::make_shared<struct meta>(name, opts, std::move(mf));
}()}
,h{[this, &name, &column]
{
	auto &handles(meta->handles);
	const auto it(std::find_if(std::begin(handles), std::end(handles), [&column]
	(const auto &cf)
	{
		return cf->GetName() == column;
	}));

	if(it != std::end(handles))
		return *it;

	log.debug("'%s': Creating new column '%s'", name, column);

	rocksdb::ColumnFamilyHandle *ret;
	throw_on_error(meta->d->CreateColumnFamily(meta->opts, column, &ret));
	meta->handles.emplace_back(ret);
	return ret;
}()}
{
}
catch(const std::exception &e)
{
	throw error("Opening handle to '%s' column '%s': %s", name, column, e);
}

db::handle::handle(handle &&other)
noexcept
:meta{std::move(other.meta)}
,h{std::move(other.h)}
{
}

db::handle &
db::handle::operator=(handle &&other)
noexcept
{
	meta = std::move(other.meta);
	h = std::move(other.h);
	return *this;
}

db::handle::~handle()
noexcept
{
}

void
db::handle::sync()
{
	throw_on_error(meta->d->SyncWAL());
}

void
db::handle::flush(const bool &blocking)
{
	rocksdb::FlushOptions opts;
	opts.wait = blocking;
	throw_on_error(meta->d->Flush(opts, h));
}

void
db::handle::del(const string_view &key,
                const sopts &sopts)
{
	using rocksdb::Slice;

	auto opts(make_opts(sopts));
	const Slice k(key.data(), key.size());
	throw_on_error(meta->d->Delete(opts, h, k));
}

void
db::handle::set(const string_view &key,
                const uint8_t *const &buf,
                const size_t &size,
                const sopts &sopts)
{
	const string_view val{reinterpret_cast<const char *>(buf), size};
	set(key, key, sopts);
}

void
db::handle::set(const string_view &key,
                const string_view &val,
                const sopts &sopts)
{
	using rocksdb::Slice;

	auto opts(make_opts(sopts));
	const Slice k(key.data(), key.size());
	const Slice v(val.data(), val.size());
	throw_on_error(meta->d->Put(opts, h, k, v));
}

std::string
db::handle::get(const string_view &key,
                const gopts &gopts)
{
	std::string ret;
	const auto copy([&ret]
	(const string_view &src)
	{
		ret.assign(begin(src), end(src));
	});

	operator()(key, copy, gopts);
	return ret;
}

size_t
db::handle::get(const string_view &key,
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

	operator()(key, copy, gopts);
	return ret;
}

size_t
db::handle::get(const string_view &key,
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

	operator()(key, copy, gopts);
	return ret;
}

namespace ircd {
namespace db   {

static
void append(rocksdb::WriteBatch &batch,
            rocksdb::ColumnFamilyHandle *const &h,
            const delta &delta)
{
	const rocksdb::Slice k
	{
		std::get<1>(delta).data(), std::get<1>(delta).size()
	};

	const rocksdb::Slice v
	{
		std::get<2>(delta).data(), std::get<2>(delta).size()
	};

	switch(std::get<0>(delta))
	{
		case op::GET:            assert(0);                    break;
		case op::SET:            batch.Put(h, k, v);           break;
		case op::MERGE:          batch.Merge(h, k, v);         break;
		case op::DELETE:         batch.Delete(h, k);           break;
		case op::DELETE_RANGE:   batch.DeleteRange(h, k, v);   break;
		case op::SINGLE_DELETE:  batch.SingleDelete(h, k);     break;
	}
}

} // namespace db
} // namespace ircd

void
db::handle::operator()(const op &op,
                       const string_view &key,
                       const string_view &val,
                       const sopts &sopts)
{
	operator()(delta{op, key, val}, sopts);
}

void
db::handle::operator()(const std::initializer_list<delta> &deltas,
                       const sopts &sopts)
{
	rocksdb::WriteBatch batch;
	for(const auto &delta : deltas)
		append(batch, h, delta);

	auto opts(make_opts(sopts));
	throw_on_error(meta->d->Write(opts, &batch));
}

void
db::handle::operator()(const delta &delta,
                       const sopts &sopts)
{
	rocksdb::WriteBatch batch;
	append(batch, h, delta);

	auto opts(make_opts(sopts));
	throw_on_error(meta->d->Write(opts, &batch));
}

void
db::handle::operator()(const string_view &key,
                       const gopts &gopts,
                       const closure &func)
{
	return operator()(key, func, gopts);
}

void
db::handle::operator()(const string_view &key,
                       const closure &func,
                       const gopts &gopts)
{
	using rocksdb::Slice;
	using rocksdb::Iterator;

	auto opts(make_opts(gopts));
	const Slice sk(key.data(), key.size());
	const auto it(seek(*meta->d, h, sk, opts));
	valid_equal_or_throw(*it, sk);
	const auto &v(it->value());
	func(string_view{v.data(), v.size()});
}

bool
db::handle::has(const string_view &key,
                const gopts &gopts)
{
	using rocksdb::Slice;
	using rocksdb::Status;

	const Slice k(key.data(), key.size());
	auto opts(make_opts(gopts));

	// Perform queries which are stymied from any sysentry
	opts.read_tier = NON_BLOCKING;

	// Perform a co-RP query to the filtration
	if(!meta->d->KeyMayExist(opts, h, k, nullptr, nullptr))
		return false;

	// Perform a query to the cache
	auto status(meta->d->Get(opts, h, k, nullptr));
	if(status.IsIncomplete())
	{
		// DB cache miss; next query requires I/O, offload it
		opts.read_tier = BLOCKING;
		ctx::offload([this, &opts, &k, &status]
		{
			status = meta->d->Get(opts, h, k, nullptr);
		});
	}

	// Finally the result
	switch(status.code())
	{
		case Status::kOk:          return true;
		case Status::kNotFound:    return false;
		default:
			throw_on_error(status);
			__builtin_unreachable();
	}
}

///////////////////////////////////////////////////////////////////////////////
//
// handle::const_iterator
//

namespace ircd {
namespace db   {

struct handle::const_iterator::state
{
	struct handle *handle;
	rocksdb::ReadOptions ropts;
	std::shared_ptr<const rocksdb::Snapshot> snap;
	std::unique_ptr<rocksdb::Iterator> it;

	state(struct handle *const & = nullptr, const gopts & = {});
};

} // namespace db
} // namespace ircd

db::handle::const_iterator
db::handle::cend(const gopts &gopts)
{
	return {};
}

db::handle::const_iterator
db::handle::cbegin(const gopts &gopts)
{
	const_iterator ret
	{
		std::make_unique<struct const_iterator::state>(this, gopts)
	};

	auto &state(*ret.state);
	if(!has_opt(gopts, db::get::READAHEAD))
		state.ropts.readahead_size = DEFAULT_READAHEAD;

	seek(*state.handle->meta->d, state.handle->h, pos::FRONT, state.ropts, state.it);
	return std::move(ret);
}

db::handle::const_iterator
db::handle::upper_bound(const string_view &key,
                        const gopts &gopts)
{
	using rocksdb::Slice;

	auto it(lower_bound(key, gopts));
	const Slice sk(key.data(), key.size());
	if(it && it.state->it->key().compare(sk) == 0)
		++it;

	return std::move(it);
}

db::handle::const_iterator
db::handle::lower_bound(const string_view &key,
                        const gopts &gopts)
{
	using rocksdb::Slice;

	const_iterator ret
	{
		std::make_unique<struct const_iterator::state>(this, gopts)
	};

	auto &state(*ret.state);
	if(!has_opt(gopts, db::get::READAHEAD))
		state.ropts.readahead_size = DEFAULT_READAHEAD;

	const Slice sk(key.data(), key.size());
	seek(*state.handle->meta->d, state.handle->h, sk, state.ropts, state.it);
	return std::move(ret);
}

db::handle::const_iterator::state::state(db::handle *const &handle,
                                         const gopts &gopts)
:handle{handle}
,ropts{make_opts(gopts, true)}
,snap
{
	[this, &handle, &gopts]() -> const rocksdb::Snapshot *
	{
		if(handle && !has_opt(gopts, db::get::NO_SNAPSHOT))
			ropts.snapshot = handle->meta->d->GetSnapshot();

		return ropts.snapshot;
	}()
	,[this](const auto *const &snap)
	{
		if(this->handle && this->handle->meta && this->handle->meta->d)
			this->handle->meta->d->ReleaseSnapshot(snap);
	}
}
{
}

db::handle::const_iterator::const_iterator(std::unique_ptr<struct state> &&state)
:state{std::move(state)}
{
}

db::handle::const_iterator::const_iterator(const_iterator &&o)
noexcept
:state{std::move(o.state)}
{
}

db::handle::const_iterator::~const_iterator()
noexcept
{
}

db::handle::const_iterator &
db::handle::const_iterator::operator--()
{
	seek(*state->handle->meta->d, state->handle->h, pos::PREV, state->ropts, state->it);
	return *this;
}

db::handle::const_iterator &
db::handle::const_iterator::operator++()
{
	seek(*state->handle->meta->d, state->handle->h, pos::NEXT, state->ropts, state->it);
	return *this;
}

const db::handle::const_iterator::value_type &
db::handle::const_iterator::operator*()
const
{
	const auto &k(state->it->key());
	const auto &v(state->it->value());

	val.first   = { k.data(), k.size() };
	val.second  = { v.data(), v.size() };

	return val;
}

const db::handle::const_iterator::value_type *
db::handle::const_iterator::operator->()
const
{
	return &operator*();
}

bool
db::handle::const_iterator::operator>=(const const_iterator &o)
const
{
	return (*this > o) || (*this == o);
}

bool
db::handle::const_iterator::operator<=(const const_iterator &o)
const
{
	return (*this < o) || (*this == o);
}

bool
db::handle::const_iterator::operator!=(const const_iterator &o)
const
{
	return !(*this == o);
}

bool
db::handle::const_iterator::operator==(const const_iterator &o)
const
{
	if(*this && o)
	{
		const auto &a(state->it->key());
		const auto &b(o.state->it->key());
		return a.compare(b) == 0;
	}

	if(!*this && !o)
		return true;

	return false;
}

bool
db::handle::const_iterator::operator>(const const_iterator &o)
const
{
	if(*this && o)
	{
		const auto &a(state->it->key());
		const auto &b(o.state->it->key());
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
db::handle::const_iterator::operator<(const const_iterator &o)
const
{
	if(*this && o)
	{
		const auto &a(state->it->key());
		const auto &b(o.state->it->key());
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
db::handle::const_iterator::operator!()
const
{
	if(!state)
		return true;

	if(!state->it)
		return true;

	if(!state->it->Valid())
		return true;

	return false;
}

db::handle::const_iterator::operator bool()
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
                       const opts &opts)
{
	return column_names(path, make_opts(opts));
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

rocksdb::Options
db::make_opts(const opts &opts)
{
	rocksdb::Options ret;
	ret.create_if_missing = true;            // They default this to false, but we invert the option

	for(const auto &o : opts) switch(o.first)
	{
		case opt::NO_CREATE:
			ret.create_if_missing = false;
			continue;

		case opt::NO_EXISTING:
			ret.error_if_exists = true;
			continue;

		case opt::NO_CHECKSUM:
			ret.paranoid_checks = false;
			continue;

		case opt::NO_MADV_DONTNEED:
			ret.allow_os_buffer = false;
			continue;

		case opt::NO_MADV_RANDOM:
			ret.advise_random_on_open = false;
			continue;

		case opt::FALLOCATE:
			ret.allow_fallocate = true;
			continue;

		case opt::NO_FALLOCATE:
			ret.allow_fallocate = false;
			continue;

		case opt::NO_FDATASYNC:
			ret.disableDataSync = true;
			continue;

		case opt::FSYNC:
			ret.use_fsync = true;
			continue;

		case opt::MMAP_READS:
			ret.allow_mmap_reads = true;
			continue;

		case opt::MMAP_WRITES:
			ret.allow_mmap_writes = true;
			continue;

		case opt::STATS_THREAD:
			ret.enable_thread_tracking = true;
			continue;

		case opt::STATS_MALLOC:
			ret.dump_malloc_stats = true;
			continue;

		case opt::OPEN_FAST:
			ret.skip_stats_update_on_db_open = true;
			continue;

		case opt::OPEN_BULKLOAD:
			ret.PrepareForBulkLoad();
			continue;

		case opt::OPEN_SMALL:
			ret.OptimizeForSmallDb();
			continue;

		default:
			continue;
	}

	return ret;
}

rocksdb::ReadOptions
db::make_opts(const gopts &opts,
              const bool &iterator)
{
	rocksdb::ReadOptions ret;

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
db::make_opts(const sopts &opts)
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

std::unique_ptr<rocksdb::Iterator>
db::seek(rocksdb::DB &db,
         rocksdb::ColumnFamilyHandle *const &h,
         const rocksdb::Slice &key,
         rocksdb::ReadOptions &opts)
{
	using rocksdb::Iterator;

	// Perform a query which won't be allowed to do kernel IO
	opts.read_tier = NON_BLOCKING;

	std::unique_ptr<Iterator> it(db.NewIterator(opts, h));
	seek(*it, key);

	if(it->status().IsIncomplete())
	{
		// DB cache miss: reset the iterator to blocking mode and offload it
		opts.read_tier = BLOCKING;
		it.reset(db.NewIterator(opts, h));
		ctx::offload([&] { seek(*it, key); });
	}
	// else DB cache hit; no context switch; no thread switch; no kernel I/O; gg

	return std::move(it);
}

template<class pos>
void
db::seek(rocksdb::DB &db,
         rocksdb::ColumnFamilyHandle *const &h,
         const pos &p,
         rocksdb::ReadOptions &opts,
         std::unique_ptr<rocksdb::Iterator> &it)
{
	// Start with a non-blocking query
	if(!it || opts.read_tier == BLOCKING)
	{
		opts.read_tier = NON_BLOCKING;
		it.reset(db.NewIterator(opts, h));
	}

	seek(*it, p);
	if(it->status().IsIncomplete())
	{
		// DB cache miss: reset the iterator to blocking mode and offload it
		opts.read_tier = BLOCKING;
		it.reset(db.NewIterator(opts, h));
		ctx::offload([&] { seek(*it, p); });
	}
}

void
db::seek(rocksdb::Iterator &it,
         const rocksdb::Slice &sk)
{
	it.Seek(sk);
}

void
db::seek(rocksdb::Iterator &it,
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
db::valid_equal_or_throw(const rocksdb::Iterator &it,
                         const rocksdb::Slice &sk)
{
	valid_or_throw(it);
	if(it.key().compare(sk) != 0)
		throw not_found();
}

void
db::valid_or_throw(const rocksdb::Iterator &it)
{
	if(!valid(it))
	{
		throw_on_error(it.status());
		throw not_found();
		//assert(0); // status == ok + !Valid() == ???
	}
}

bool
db::valid(const rocksdb::Iterator &it)
{
	return it.Valid();
}

void
db::throw_on_error(const rocksdb::Status &s)
{
	using rocksdb::Status;

	switch(s.code())
	{
		case Status::kOk:                   return;
		case Status::kNotFound:             throw not_found();
		case Status::kCorruption:           throw corruption();
		case Status::kNotSupported:         throw not_supported();
		case Status::kInvalidArgument:      throw invalid_argument();
		case Status::kIOError:              throw io_error();
		case Status::kMergeInProgress:      throw merge_in_progress();
		case Status::kIncomplete:           throw incomplete();
		case Status::kShutdownInProgress:   throw shutdown_in_progress();
		case Status::kTimedOut:             throw timed_out();
		case Status::kAborted:              throw aborted();
		case Status::kBusy:                 throw busy();
		case Status::kExpired:              throw expired();
		case Status::kTryAgain:             throw try_again();
		default:
			throw error("Unknown error");
	}
}

std::string
db::path(const std::string &name)
{
	const auto prefix(path::get(path::DB));
	return path::build({prefix, name});
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
