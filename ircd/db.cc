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
template<class pos> void seek(rocksdb::DB &, const pos &, rocksdb::ReadOptions &, std::unique_ptr<rocksdb::Iterator> &);
std::unique_ptr<rocksdb::Iterator> seek(rocksdb::DB &, const rocksdb::Slice &, rocksdb::ReadOptions &);

// This is important to prevent thrashing the iterators which have to reset on iops
const auto DEFAULT_READAHEAD = 4_MiB;

rocksdb::WriteOptions make_opts(const sopts &);
rocksdb::ReadOptions make_opts(const gopts &, const bool &iterator = false);
rocksdb::Options make_opts(const opts &);

struct meta
:rocksdb::Statistics
,rocksdb::EventListener
,rocksdb::AssociativeMergeOperator
,rocksdb::Logger
{
	std::string name;
	std::string path;
	rocksdb::Options opts;
	std::shared_ptr<rocksdb::Cache> cache;
	std::vector<rocksdb::ColumnFamilyDescriptor> cols;
	std::vector<rocksdb::ColumnFamilyHandle *> handles;
	merge_function merger;

	// Statistics
	std::array<uint64_t, rocksdb::TICKER_ENUM_MAX> ticker {{0}};
	std::array<rocksdb::HistogramData, rocksdb::HISTOGRAM_ENUM_MAX> histogram;

	uint64_t getTickerCount(const uint32_t tickerType) const override;
	void recordTick(const uint32_t tickerType, const uint64_t count) override;
	void setTickerCount(const uint32_t tickerType, const uint64_t count) override;
	void histogramData(const uint32_t type, rocksdb::HistogramData *) const override;
	void measureTime(const uint32_t histogramType, const uint64_t time) override;
	bool HistEnabledForType(const uint32_t type) const override;

	// EventListener
	void OnFlushCompleted(rocksdb::DB *, const rocksdb::FlushJobInfo &) override;
	void OnCompactionCompleted(rocksdb::DB *, const rocksdb::CompactionJobInfo &) override;
	void OnTableFileDeleted(const rocksdb::TableFileDeletionInfo &) override;
	void OnTableFileCreated(const rocksdb::TableFileCreationInfo &) override;
	void OnTableFileCreationStarted(const rocksdb::TableFileCreationBriefInfo &) override;
	void OnMemTableSealed(const rocksdb::MemTableInfo &) override;
	void OnColumnFamilyHandleDeletionStarted(rocksdb::ColumnFamilyHandle *) override;

	// AssociativeMergeOperator
	bool Merge(const rocksdb::Slice &, const rocksdb::Slice *, const rocksdb::Slice &, std::string *, rocksdb::Logger *) const override;
	const char *Name() const override;

	// Logger
	void Logv(const rocksdb::InfoLogLevel level, const char *fmt, va_list ap) override;
	void Logv(const char *fmt, va_list ap) override;
	void LogHeader(const char *fmt, va_list ap) override;

	meta();
	~meta() noexcept;
};

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

namespace ircd {
namespace db   {

} // namespace db
} // namespace ircd

db::handle::handle(const std::string &name,
                   const opts &opts,
                   merge_function mf)
try
:meta{[&name, &opts, &mf]
{
	auto meta(std::make_shared<struct meta>());
	meta->name = name;
	meta->path = path(name);
	meta->opts = make_opts(opts);

	// Setup logging;
	meta->opts.info_log = meta;
	meta->SetInfoLogLevel(ircd::debugmode? rocksdb::DEBUG_LEVEL : rocksdb::WARN_LEVEL);
	meta->opts.info_log_level = meta->GetInfoLogLevel();

	meta->opts.create_missing_column_families = true;
	meta->cols =
	{
		rocksdb::ColumnFamilyDescriptor { "default", meta->opts },
	};

	// Setup event and statistics callbacks
	meta->opts.listeners.emplace_back(meta);
	//meta->opts.statistics = meta;              // broken?

	// Setup performance metric options
	//rocksdb::SetPerfLevel(rocksdb::PerfLevel::kDisable);

	// Setup journal recovery options
	meta->opts.wal_recovery_mode = rocksdb::WALRecoveryMode::kAbsoluteConsistency;
	//meta->opts.wal_recovery_mode = rocksdb::WALRecoveryMode::kPointInTimeRecovery;

	// Setup caching options
	const auto lru_cache_size(opt_val(opts, opt::LRU_CACHE));
	if(lru_cache_size > 0)
		meta->cache = rocksdb::NewLRUCache(lru_cache_size);
	meta->opts.row_cache = meta->cache;

	if(mf) // Setup user operators
	{
		meta->merger = std::move(mf);
		meta->opts.merge_operator = meta;
	}

	return meta;
}()}
,d{[this, &opts]
{
	auto &name(meta->name);
	auto &path(meta->path);
	auto &cols(meta->cols);
	auto &handles(meta->handles);
	log.debug("Attempting to open database \"%s\" @ `%s'",
	          name,
	          path);

	rocksdb::DB *ptr;
	if(has_opt(opts, opt::READ_ONLY))
		throw_on_error(rocksdb::DB::OpenForReadOnly(meta->opts, path, cols, &handles, &ptr));
	else
		throw_on_error(rocksdb::DB::Open(meta->opts, path, cols, &handles, &ptr));

	return std::unique_ptr<rocksdb::DB>{ptr};
}()}
{
	log.info("Opened database \"%s\" @ `%s' (handle: %p) columns: %zu",
	         meta->name,
	         meta->path,
	         (const void *)this,
	         meta->handles.size());
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

	throw error("Failed to open db '%s': %s%s",
	            name,
	            e.what(),
	            helpstr);
}
catch(const std::exception &e)
{
	throw error("Failed to open db '%s': %s",
	            name,
	            e.what());
}

db::handle::handle()
{
}

db::handle::handle(handle &&other)
noexcept
:meta{std::move(other.meta)}
,d{std::move(other.d)}
{
}

db::handle &
db::handle::operator=(handle &&other)
noexcept
{
	meta = std::move(other.meta);
	d = std::move(other.d);
	return *this;
}

db::handle::~handle()
noexcept
{
	if(!*this)
		return;

	// Branch not taken after std::move()
	log.info("Closing database \"%s\" @ `%s' (handle: %p)",
	         meta->name.c_str(),
	         meta->path.c_str(),
	         (const void *)this);

	for(const auto &handle : meta->handles)
		d->DestroyColumnFamilyHandle(handle);
}

void
db::handle::del(const string_view &key,
                const sopts &sopts)
{
	using rocksdb::Slice;

	auto opts(make_opts(sopts));
	const Slice k(key.data(), key.size());
	throw_on_error(d->Delete(opts, k));
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
	throw_on_error(d->Put(opts, k, v));
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
		case op::GET:            assert(0);                 break;
		case op::SET:            batch.Put(k, v);           break;
		case op::MERGE:          batch.Merge(k, v);         break;
		case op::DELETE:         batch.Delete(k);           break;
		case op::DELETE_RANGE:   batch.DeleteRange(k, v);   break;
		case op::SINGLE_DELETE:  batch.SingleDelete(k);     break;
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
		append(batch, delta);

	auto opts(make_opts(sopts));
	throw_on_error(d->Write(opts, &batch));
}

void
db::handle::operator()(const delta &delta,
                       const sopts &sopts)
{
	rocksdb::WriteBatch batch;
	append(batch, delta);

	auto opts(make_opts(sopts));
	throw_on_error(d->Write(opts, &batch));
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
	const auto it(seek(*d, sk, opts));
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
	if(!d->KeyMayExist(opts, k, nullptr, nullptr))
		return false;

	// Perform a query to the cache
	auto status(d->Get(opts, k, nullptr));
	if(status.IsIncomplete())
	{
		// DB cache miss; next query requires I/O, offload it
		opts.read_tier = BLOCKING;
		ctx::offload([this, &opts, &k, &status]
		{
			status = d->Get(opts, k, nullptr);
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
// Meta
//

ircd::db::meta::meta()
{
}

ircd::db::meta::~meta()
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
ircd::db::meta::Logv(const char *const fmt,
                     va_list ap)
{
	Logv(rocksdb::InfoLogLevel::DEBUG_LEVEL, fmt, ap);
}

void
ircd::db::meta::LogHeader(const char *const fmt,
                          va_list ap)
{
	Logv(rocksdb::InfoLogLevel::DEBUG_LEVEL, fmt, ap);
}

void
ircd::db::meta::Logv(const rocksdb::InfoLogLevel level,
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

	log(translate(level), "'%s': (rdb) %s", name, str);
}

const char *
ircd::db::meta::Name()
const
{
	return "<unnamed>";
}

bool
ircd::db::meta::Merge(const rocksdb::Slice &_key,
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
	log.critical("merge: missing merge operator (%s)", e.what());
	return false;
}
catch(const std::exception &e)
{
	log.error("merge: %s", e.what());
	return false;
}


bool
ircd::db::meta::HistEnabledForType(const uint32_t type)
const
{
	return type < histogram.size();
}

void
ircd::db::meta::measureTime(const uint32_t type,
                            const uint64_t time)
{
}

void
ircd::db::meta::histogramData(const uint32_t type,
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
ircd::db::meta::recordTick(const uint32_t type,
                           const uint64_t count)
{
	ticker.at(type) += count;
}

void
ircd::db::meta::setTickerCount(const uint32_t type,
                               const uint64_t count)
{
	ticker.at(type) = count;
}

uint64_t
ircd::db::meta::getTickerCount(const uint32_t type)
const
{
	return ticker.at(type);
}

void
ircd::db::meta::OnFlushCompleted(rocksdb::DB *const db,
                                 const rocksdb::FlushJobInfo &info)
{
	log.debug("'%s' @%p: flushed: column[%s] path[%s] tid[%lu] job[%d] writes[slow:%d stop:%d]",
	          name,
	          db,
	          info.cf_name,
	          info.file_path,
	          info.thread_id,
	          info.job_id,
	          info.triggered_writes_slowdown,
	          info.triggered_writes_stop);
}

void
ircd::db::meta::OnCompactionCompleted(rocksdb::DB *const db,
                                      const rocksdb::CompactionJobInfo &info)
{
	log.debug("'%s' @%p: compacted: column[%s] status[%d] tid[%lu] job[%d]",
	          name,
	          db,
	          info.cf_name,
	          int(info.status.code()),
	          info.thread_id,
	          info.job_id);
}

void
ircd::db::meta::OnTableFileDeleted(const rocksdb::TableFileDeletionInfo &info)
{
	log.debug("'%s': table file deleted: db[%s] path[%s] status[%d] job[%d]",
	          name,
	          info.db_name,
	          info.file_path,
	          int(info.status.code()),
	          info.job_id);
}

void
ircd::db::meta::OnTableFileCreated(const rocksdb::TableFileCreationInfo &info)
{
	log.debug("'%s': table file created: db[%s] path[%s] status[%d] job[%d]",
	          name,
	          info.db_name,
	          info.file_path,
	          int(info.status.code()),
	          info.job_id);
}

void
ircd::db::meta::OnTableFileCreationStarted(const rocksdb::TableFileCreationBriefInfo &info)
{
	log.debug("'%s': table file creating: db[%s] column[%s] path[%s] job[%d]",
	          name,
	          info.db_name,
	          info.cf_name,
	          info.file_path,
	          info.job_id);
}

void
ircd::db::meta::OnMemTableSealed(const rocksdb::MemTableInfo &info)
{
	log.debug("'%s': memory table sealed: column[%s] entries[%lu] deletes[%lu]",
	          name,
	          info.cf_name,
	          info.num_entries,
	          info.num_deletes);
}

void
ircd::db::meta::OnColumnFamilyHandleDeletionStarted(rocksdb::ColumnFamilyHandle *const h)
{
	log.debug("'%s': column family handle deletion started: %p",
	          name,
	          h);
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

	seek(*state.handle->d, pos::FRONT, state.ropts, state.it);
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
	seek(*state.handle->d, sk, state.ropts, state.it);
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
			ropts.snapshot = handle->d->GetSnapshot();

		return ropts.snapshot;
	}()
	,[this](const auto *const &snap)
	{
		if(this->handle && this->handle->d)
			this->handle->d->ReleaseSnapshot(snap);
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
	seek(*state->handle->d, pos::PREV, state->ropts, state->it);
	return *this;
}

db::handle::const_iterator &
db::handle::const_iterator::operator++()
{
	seek(*state->handle->d, pos::NEXT, state->ropts, state->it);
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
         const rocksdb::Slice &key,
         rocksdb::ReadOptions &opts)
{
	using rocksdb::Iterator;

	// Perform a query which won't be allowed to do kernel IO
	opts.read_tier = NON_BLOCKING;

	std::unique_ptr<Iterator> it(db.NewIterator(opts));
	seek(*it, key);

	if(it->status().IsIncomplete())
	{
		// DB cache miss: reset the iterator to blocking mode and offload it
		opts.read_tier = BLOCKING;
		it.reset(db.NewIterator(opts));
		ctx::offload([&] { seek(*it, key); });
	}
	// else DB cache hit; no context switch; no thread switch; no kernel I/O; gg

	return std::move(it);
}

template<class pos>
void
db::seek(rocksdb::DB &db,
         const pos &p,
         rocksdb::ReadOptions &opts,
         std::unique_ptr<rocksdb::Iterator> &it)
{
	// Start with a non-blocking query
	if(!it || opts.read_tier == BLOCKING)
	{
		opts.read_tier = NON_BLOCKING;
		it.reset(db.NewIterator(opts));
	}

	seek(*it, p);
	if(it->status().IsIncomplete())
	{
		// DB cache miss: reset the iterator to blocking mode and offload it
		opts.read_tier = BLOCKING;
		it.reset(db.NewIterator(opts));
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
