// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include "db.h"

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
	"db.rocksdb"
};

decltype(ircd::db::version_api)
ircd::db::version_api
{
	"RocksDB", info::versions::API, 0,
	{
		ROCKSDB_MAJOR, ROCKSDB_MINOR, ROCKSDB_PATCH,
	}
};

extern "C" const char *
rocksdb_build_git_sha;

extern "C" const char *
rocksdb_build_compile_date;

decltype(ircd::db::version_abi)
ircd::db::version_abi
{
	"RocksDB", info::versions::ABI, 0, {0}, []
	(auto &, const mutable_buffer &buf)
	{
		fmt::sprintf
		{
			buf, "%s (%s)",
			lstrip(rocksdb_build_git_sha, "rocksdb_build_git_sha:"),
			rocksdb_build_compile_date,
		};
	}
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
		{ "default",  0L                          },
	}, []
	{
		request.set(size_t(request_pool_size));
	}
};

decltype(ircd::db::request_pool_opts)
ircd::db::request_pool_opts
{
	size_t(request_pool_stack_size),
	size_t(request_pool_size),
	-1,   // No hard limit
	0,    // Soft limit at any queued
	true, // Yield before hitting soft limit
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
	"db req", request_pool_opts
};

///////////////////////////////////////////////////////////////////////////////
//
// init
//

decltype(ircd::db::init::direct_io_test_file_path)
ircd::db::init::direct_io_test_file_path
{
	fs::path_string(fs::path_views
	{
		fs::base::db, "SUPPORTS_DIRECT_IO"_sv
	})
};

ircd::db::init::init()
try
{
	#ifdef IRCD_DB_HAS_ALLOCATOR
	database::allocator::init();
	#endif
	compressions();
	directory();
	request_pool();
	test_direct_io();
	test_hw_crc32();
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "Cannot start database system :%s",
		e.what()
	};

	throw;
}

ircd::db::init::~init()
noexcept
{
	delete prefetcher;
	prefetcher = nullptr;

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

	#ifdef IRCD_DB_HAS_ALLOCATOR
	database::allocator::fini();
	#endif
}

void
ircd::db::init::directory()
try
{
	const string_view &dbdir
	{
		fs::base::db
	};

	if(!fs::is_dir(dbdir) && (ircd::read_only || ircd::write_avoid))
		log::warning
		{
			log, "Not creating database directory `%s' in read-only/write-avoid mode.", dbdir
		};
	else if(fs::mkdir(dbdir))
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
		log, "Database directory error: %s", e.what()
	};

	throw;
}

void
ircd::db::init::test_direct_io()
try
{
	const auto &test_file_path
	{
		direct_io_test_file_path
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
			string_view{fs::base::db}
		};
}
catch(const std::exception &e)
{
	log::error
	{
		log, "Failed to test if Direct-IO possible with test file `%s'"
		"; Concurrent database queries will not be possible :%s",
		direct_io_test_file_path,
		e.what()
	};
}

namespace rocksdb::crc32c
{
	extern std::string IsFastCrc32Supported();
}

void
ircd::db::init::test_hw_crc32()
try
{
	const auto supported_str
	{
		rocksdb::crc32c::IsFastCrc32Supported()
	};

	const bool supported
	{
		startswith(supported_str, "Supported")
	};

	assert(supported || startswith(supported_str, "Not supported"));

	if(!supported)
		log::warning
		{
			log, "crc32c hardware acceleration is not available on this platform."
		};
}
catch(const std::exception &e)
{
	log::error
	{
		log, "Failed to test crc32c hardware acceleration support :%s",
		e.what()
	};
}

decltype(ircd::db::compressions)
ircd::db::compressions;

void
ircd::db::init::compressions()
try
{
	auto supported
	{
		rocksdb::GetSupportedCompressions()
	};

	size_t i(0);
	for(const rocksdb::CompressionType &type_ : supported) try
	{
		auto &[string, type]
		{
			db::compressions.at(i++)
		};

		type = type_;
		throw_on_error
		{
			rocksdb::GetStringFromCompressionType(&string, type_)
		};

		log::debug
		{
			log, "Detected supported compression #%zu type:%lu :%s",
			i,
			type,
			string,
		};
	}
	catch(const std::exception &e)
	{
		log::error
		{
			log, "Failed to identify compression type:%u :%s",
			uint(type_),
			e.what()
		};
	}

	if(supported.empty())
		log::warning
		{
			"No compression libraries have been linked with the DB."
			" This is probably not what you want."
		};
}
catch(const std::exception &e)
{
	log::error
	{
		log, "Failed to initialize database compressions :%s",
		e.what()
	};

	throw;
}

void
ircd::db::init::request_pool()
{
	char buf[32];
	const string_view value
	{
		conf::get(buf, "ircd.fs.aio.max_events")
	};

	const size_t aio_max_events
	{
		lex_castable<size_t>(value)?
			lex_cast<size_t>(value):
			0UL
	};

	const size_t new_size
	{
		size_t(request_pool_size)?
			request_pool_size:
		aio_max_events?
			aio_max_events:
			1UL
	};

	request_pool_size.set(lex_cast(new_size));
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
// db/prefetcher.h
//

decltype(ircd::db::prefetcher)
ircd::db::prefetcher;

//
// db::prefetcher
//
ircd::db::prefetcher::prefetcher()
:ticker
{
	std::make_unique<struct ticker>()
}
,context
{
	"db.prefetcher",
	256_KiB,
	context::POST,
	std::bind(&prefetcher::worker, this)
}
{
}

ircd::db::prefetcher::~prefetcher()
noexcept
{
	while(!queue.empty())
	{
		log::warning
		{
			log, "Prefetcher waiting for %zu requests to clear...",
			queue.size(),
		};

		dock.wait_for(seconds(5), [this]
		{
			return queue.empty();
		});
	}

	assert(queue.empty());
}

bool
ircd::db::prefetcher::operator()(column &c,
                                 const string_view &key,
                                 const gopts &opts)
{
	auto &d
	{
		static_cast<database &>(c)
	};

	assert(ticker);
	ticker->queries++;
	if(db::cached(c, key, opts))
	{
		ticker->rejects++;
		return false;
	}

	queue.emplace_back(d, c, key);
	queue.back().snd = now<steady_point>();
	ticker->request++;

	// Branch here based on whether it's not possible to directly dispatch
	// a db::request worker. If all request workers are busy we notify our own
	// prefetcher worker, and then it blocks on submitting to the request
	// worker instead of us blocking here. This is done to avoid use and growth
	// of any request pool queue, and allow for more direct submission.
	if(db::request.wouldblock())
	{
		dock.notify_one();

		// If the user sets NO_BLOCKING we honor their request to not
		// context switch for a prefetch. However by default we want to
		// control queue growth, so we insert voluntary yield here to allow
		// prefetch operations to at least be processed before returning to
		// the user submitting more prefetches.
		if(likely(!test(opts, db::get::NO_BLOCKING)))
			ctx::yield();

		return true;
	}

	const ctx::critical_assertion ca;
	ticker->directs++;
	this->handle();
	return true;
}

size_t
ircd::db::prefetcher::cancel(column &c)
{
	return cancel([&c]
	(const auto &request)
	{
		return request.cid == id(c);
	});
}

size_t
ircd::db::prefetcher::cancel(database &d)
{
	return cancel([&d]
	(const auto &request)
	{
		return request.d == std::addressof(d);
	});
}

size_t
ircd::db::prefetcher::cancel(const closure &closure)
{
	size_t canceled(0);
	for(auto &request : queue)
	{
		// already finished
		if(request.fin != steady_point::min())
			continue;

		// in progress; can't cancel
		if(request.req != steady_point::min())
			continue;

		// allow user to accept or reject
		if(!closure(request))
			continue;

		// cancel by precociously setting the finish time.
		request.fin = now<steady_point>();
		++canceled;
	}

	if(canceled)
		dock.notify_all();

	assert(ticker);
	ticker->cancels += canceled;
	return canceled;
}

void
ircd::db::prefetcher::worker()
try
{
	while(1)
	{
		dock.wait([this]
		{
			if(queue.empty())
				return false;

			assert(ticker);
			if(ticker->request <= ticker->handles)
				return false;

			return true;
		});

		handle();
	}
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "prefetcher worker: %s",
		e.what()
	};
}

void
ircd::db::prefetcher::handle()
{
	auto handler
	{
		std::bind(&prefetcher::request_worker, this)
	};

	ticker->handles++;
	db::request(std::move(handler));
	ticker->handled++;
}

void
ircd::db::prefetcher::request_worker()
{
	const ctx::scope_notify notify
	{
		this->dock
	};

	const scope_count request_workers
	{
		this->request_workers
	};

	// Garbage collection of the queue invoked unconditionally on unwind.
	const unwind cleanup_on_leave
	{
		std::bind(&prefetcher::request_cleanup, this)
	};

	// GC the queue here to get rid of any cancelled requests which have
	// arrived at the front so they don't become our request.
	const size_t cleanup_on_enter
	{
		request_cleanup()
	};

	// Find the first request in the queue which does not have its req
	// timestamp sent.
	auto request
	{
		std::find_if(begin(queue), end(queue), []
		(const auto &request)
		{
			return request.req == steady_point::min();
		})
	};

	if(request == end(queue))
		return;

	assert(ticker);
	assert(request->fin == steady_point::min());
	request->req = now<steady_point>();
	ticker->last_snd_req = duration_cast<microseconds>(request->req - request->snd);
	static_cast<microseconds &>(ticker->accum_snd_req) += ticker->last_snd_req;

	ticker->fetches++;
	request_handle(*request);
	assert(request->fin != steady_point::min());
	ticker->fetched++;

	#ifdef IRCD_DB_DEBUG_PREFETCH
	log::debug
	{
		log, "prefetcher reject:%zu request:%zu handle:%zu fetch:%zu direct:%zu cancel:%zu queue:%zu rw:%zu",
		ticker->rejects,
		ticker->request,
		ticker->handles,
		ticker->fetches,
		ticker->directs,
		ticker->cancels,
		queue.size(),
		this->request_workers,
	};
	#endif
}

size_t
ircd::db::prefetcher::request_cleanup()
noexcept
{
	size_t removed(0);
	const ctx::critical_assertion ca;
	for(; !queue.empty() && queue.front().fin != steady_point::min(); ++removed)
		queue.pop_front();

	return removed;
}

void
ircd::db::prefetcher::request_handle(request &request)
try
{
	assert(request.d);
	db::column column
	{
		(*request.d)[request.cid]
	};

	const string_view key
	{
		request
	};

	const auto it
	{
		seek(column, key, gopts{})
	};

	const ctx::critical_assertion ca;
	request.fin = now<steady_point>();
	ticker->last_req_fin = duration_cast<microseconds>(request.fin - request.req);
	static_cast<microseconds &>(ticker->accum_req_fin) += ticker->last_req_fin;
	const bool lte
	{
		valid_lte(*it, key)
	};

	if(likely(lte))
	{
		ticker->fetched_bytes_key += size(it->key());
		ticker->fetched_bytes_val += size(it->value());
	}

	#ifdef IRCD_DB_DEBUG_PREFETCH
	char pbuf[3][32];
	log::debug
	{
		log, "[%s][%s] completed prefetch len:%zu lte:%b k:%zu v:%zu snd-req:%s req-fin:%s snd-fin:%s queue:%zu",
		name(*request.d),
		name(column),
		size(key),
		lte,
		lte? size(it->key()) : 0UL,
		lte? size(it->value()) : 0UL,
		pretty(pbuf[0], request.req - request.snd, 1),
		pretty(pbuf[1], request.fin - request.req, 1),
		pretty(pbuf[2], request.fin - request.snd, 1),
		queue.size(),
	};
	#endif
}
catch(const std::exception &e)
{
	assert(request.d);
	request.fin = now<steady_point>();

	log::error
	{
		log, "[%s][%u] :%s",
		name(*request.d),
		request.cid,
		e.what(),
	};
}
catch(...)
{
	request.fin = now<steady_point>();
	throw;
}

size_t
ircd::db::prefetcher::wait_pending()
{
	const size_t fetched_counter
	{
		ticker->fetched
	};

	const size_t fetched_target
	{
		fetched_counter + request_workers
	};

	dock.wait([this, &fetched_target]
	{
		return this->ticker->fetched >= fetched_target;
	});

	assert(fetched_target >= fetched_counter);
	return fetched_target - fetched_counter;
}

//
// prefetcher::request
//

ircd::db::prefetcher::request::request(database &d,
                                       const column &c,
                                       const string_view &key)
noexcept
:d
{
	std::addressof(d)
}
,cid
{
	db::id(c)
}
,len
{
	 uint32_t(std::min(size(key), sizeof(this->key)))
}
,snd
{
	steady_point::min()
}
,req
{
	steady_point::min()
}
,fin
{
	steady_point::min()
}
{
	const size_t &len
	{
		buffer::copy(this->key, key)
	};

	assert(this->len == len);
}

ircd::db::prefetcher::request::operator
ircd::string_view()
const noexcept
{
	return
	{
		key, len
	};
}

//
// prefetcher::ticker
//

ircd::db::prefetcher::ticker::ticker()
:queries
{
	{ "name", "ircd.db.prefetch.queries" },
}
,rejects
{
	{ "name", "ircd.db.prefetch.rejects" },
}
,request
{
	{ "name", "ircd.db.prefetch.request" },
}
,directs
{
	{ "name", "ircd.db.prefetch.directs" },
}
,handles
{
	{ "name", "ircd.db.prefetch.handles" },
}
,handled
{
	{ "name", "ircd.db.prefetch.handled" },
}
,fetches
{
	{ "name", "ircd.db.prefetch.fetches" },
}
,fetched
{
	{ "name", "ircd.db.prefetch.fetched" },
}
,cancels
{
	{ "name", "ircd.db.prefetch.cancels" },
}
,fetched_bytes_key
{
	{ "name", "ircd.db.prefetch.fetched_bytes_key" },
}
,fetched_bytes_val
{
	{ "name", "ircd.db.prefetch.fetched_bytes_val" },
}
,last_snd_req
{
	{ "name", "ircd.db.prefetch.last_snd_req" },
}
,last_req_fin
{
	{ "name", "ircd.db.prefetch.last_req_fin" },
}
,accum_snd_req
{
	{ "name", "ircd.db.prefetch.accum_snd_req" },
}
,accum_req_fin
{
	{ "name", "ircd.db.prefetch.accum_req_fin" },
}
{
}

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

ircd::string_view
ircd::db::debug(const mutable_buffer &buf,
                database &d,
                const rocksdb::WriteBatch &wb_,
                const ulong &fmt)
{
	auto &wb
	{
		mutable_cast(wb_)
	};

	txn t
	{
		d, std::unique_ptr<rocksdb::WriteBatch>{&wb}
	};

	const unwind release
	{
		std::bind(&txn::release, &t)
	};

	return debug(buf, t, fmt);
}

ircd::string_view
ircd::db::debug(const mutable_buffer &buf,
                const txn &t,
                const ulong &fmt)
{
	size_t len(0);

	if(fmt >= 0)
	{
		const rocksdb::WriteBatch &wb(t);
		len += size(db::debug(buf, wb));
	}

	if(fmt == 1)
	{
		for_each(t, [&buf, &len]
		(const delta &d)
		{
			char pbuf[2][64];
			len += copy(buf + len, '\n');
			len += fmt::sprintf
			{
				buf + len, "%18s %-12s | [%s...] %-20s => [%s...] %-20s",
				std::get<delta::COL>(d),
				reflect(std::get<delta::OP>(d)),
				"????????"_sv, //std::get<d.KEY>(d),
				pretty(pbuf[0], iec(size(std::get<delta::KEY>(d)))),
				"????????"_sv, //std::get<d.VAL>(d),
				pretty(pbuf[1], iec(size(std::get<delta::VAL>(d)))),
			};
		});

		len += copy(buf + len, '\n');
	}

	return string_view
	{
		data(buf), len
	};
}

void
ircd::db::for_each(const txn &t,
                   const delta_closure &closure)
{
	const auto re{[&closure]
	(const delta &delta)
	{
		closure(delta);
		return true;
	}};

	const database &d(t);
	const rocksdb::WriteBatch &wb(t);
	txn::handler h{d, re};
	wb.Iterate(&h);
}

bool
ircd::db::for_each(const txn &t,
                   const delta_closure_bool &closure)
{
	const database &d(t);
	const rocksdb::WriteBatch &wb(t);
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
	ircd::not_implemented{};
	return Status::OK();
}

rocksdb::Status
ircd::db::txn::handler::MarkEndPrepare(const Slice &xid)
noexcept
{
	ircd::not_implemented{};
	return Status::OK();
}

rocksdb::Status
ircd::db::txn::handler::MarkCommit(const Slice &xid)
noexcept
{
	ircd::not_implemented{};
	return Status::OK();
}

rocksdb::Status
ircd::db::txn::handler::MarkRollback(const Slice &xid)
noexcept
{
	ircd::not_implemented{};
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
		log, "txn::handler: cfid[%u]: %s",
		cfid,
		e.what()
	};

	ircd::terminate();
	__builtin_unreachable();
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
	assert(this->state == state::BUILD);
	this->state = state::COMMIT;
	commit(d, *wb, opts);
	this->state = state::COMMITTED;
}

void
ircd::db::txn::clear()
{
	assert(bool(wb));
	wb->Clear();
	this->state = state::BUILD;
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
		return std::get<delta::OP>(delta) != op &&
		       std::get<delta::COL>(delta) != col;
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
		if(std::get<delta::OP>(delta) == op &&
		   std::get<delta::COL>(delta) == col)
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
		return std::get<delta::OP>(delta) != op &&
		       std::get<delta::COL>(delta) != col &&
		       std::get<delta::KEY>(delta) != key;
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
		if(std::get<delta::OP>(delta) == op &&
		   std::get<delta::COL>(delta) == col &&
		   std::get<delta::KEY>(delta) == key)
		{
			closure(std::get<delta::VAL>(delta));
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
// txn::checkpoint
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

//
// txn::append
//

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

__attribute__((noreturn))
ircd::db::txn::append::append(txn &t,
                              const row::delta &delta)
{
	throw ircd::not_implemented
	{
		"db::txn::append (row::delta)"
	};
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
	db::column c
	{
		d[std::get<1>(delta)]
	};

	db::append(*t.wb, c, db::column::delta
	{
		std::get<op>(delta),
		std::get<2>(delta),
		std::get<3>(delta)
	});
}

///////////////////////////////////////////////////////////////////////////////
//
// db/row.h
//

namespace ircd::db
{
	static std::vector<rocksdb::Iterator *>
	_make_iterators(database &d,
	                database::column *const *const &columns,
	                const size_t &columns_size,
	                const rocksdb::ReadOptions &opts);
}

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
               const string_view &key,
               const gopts &opts)
{
	// The following closure performs the seek() for a single cell in the row.
	// It may be executed on another ircd::ctx if the data isn't cached and
	// blocking IO is required. This frame can't be interrupted because it may
	// have requests pending in the request pool which must synchronize back
	// here.
	size_t ret{0};
	std::exception_ptr eptr;
	ctx::latch latch{r.size()};
	const ctx::uninterruptible ui;
	const auto closure{[&opts, &latch, &ret, &key, &eptr]
	(auto &cell) noexcept
	{
		// If there's a pending error from another cell by the time this
		// closure is executed we don't perform the seek() unless the user
		// specifies db::get::NO_THROW to suppress it.
		if(!eptr || test(opts, get::NO_THROW)) try
		{
			if(!seek(cell, key))
			{
				// If the cell is not_found that's not a thrown exception here;
				// the cell will just be !valid(). The user can specify
				// get::THROW to propagate a not_found from the seek(row);
				if(test(opts, get::THROW))
					throw not_found
					{
						"column '%s' key '%s'", cell.col(), key
					};
			}
			else ++ret;
		}
		catch(const not_found &e)
		{
			eptr = std::current_exception();
		}
		catch(const std::exception &e)
		{
			log::error
			{
				log, "row seek: column '%s' key '%s' :%s",
				cell.col(),
				key,
				e.what()
			};

			eptr = std::make_exception_ptr(e);
		}

		// The latch must always be hit here. No exception should propagate
		// to prevent this from being reached or beyond.
		latch.count_down();
	}};

	#ifdef RB_DEBUG_DB_SEEK_ROW
	const ircd::timer timer;
	size_t submits{0};
	#endif

	// Submit all the requests
	for(auto &cell : r)
	{
		db::column &column(cell);
		const auto reclosure{[&closure, &cell]
		() noexcept
		{
			closure(cell);
		}};

		// Whether to submit the request to another ctx or execute it here.
		// Explicit option to prevent submitting must not be set. If there
		// is a chance the data is already in the cache, we can avoid the
		// context switching and occupation of the request pool.
		//TODO: should check a bloom filter on the cache for this branch
		//TODO: because right now double-querying the cache is gross.
		const bool submit
		{
			r.size() > 1 &&
			!test(opts, get::NO_PARALLEL) &&
			!db::cached(column, key, opts)
		};

		#ifdef RB_DEBUG_DB_SEEK_ROW
		submits += submit;
		#endif

		if(submit)
			request(reclosure);
		else
			reclosure();
	}

	// Wait for responses.
	latch.wait();
	assert(ret <= r.size());

	#ifdef RB_DEBUG_DB_SEEK_ROW
	if(likely(!r.empty()))
	{
		const column &c(r[0]);
		const database &d(c);
		thread_local char tmbuf[32];
		log::debug
		{
			log, "'%s' SEEK ROW seq:%lu:%-10lu cnt:%-2zu req:%-2zu ret:%-2zu in %s %s",
			name(d),
			sequence(d),
			sequence(opts.snapshot),
			r.size(),
			submits,
			ret,
			pretty(tmbuf, timer.at<microseconds>(), true),
			what(eptr)
		};
	}
	#endif

	if(eptr && !test(opts, get::NO_THROW))
		std::rethrow_exception(eptr);

	return ret;
}

//
// row
//
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstack-usage="
__attribute__((stack_protect))
ircd::db::row::row(database &d,
                   const string_view &key,
                   const vector_view<const string_view> &colnames,
                   const vector_view<cell> &buf,
                   gopts opts)
:vector_view<cell>{[&d, &colnames, &buf, &opts]
{
	using std::end;
	using std::begin;

	if(!opts.snapshot)
		opts.snapshot = database::snapshot(d);

	const rocksdb::ReadOptions options
	{
		make_opts(opts)
	};

	assert(buf.size() >= colnames.size());
	const size_t request_count
	{
		std::min(colnames.size(), buf.size())
	};

	size_t count(0);
	database::column *colptr[request_count];
	for(size_t i(0); i < request_count; ++i)
	{
		const auto cfid
		{
			d.cfid(std::nothrow, colnames.at(i))
		};

		if(cfid >= 0)
			colptr[count++] = &d[cfid];
	}

	// All pointers returned by rocksdb in this vector must be free'd.
	const auto iterators
	{
		_make_iterators(d, colptr, count, options)
	};

	assert(iterators.size() == count);
	for(size_t i(0); i < iterators.size(); ++i)
	{
		std::unique_ptr<rocksdb::Iterator> it
		{
			iterators.at(i)
		};

		buf[i] = cell
		{
			*colptr[i], std::move(it), opts
		};
	}

	return vector_view<cell>
	{
		buf.data(), iterators.size()
	};
}()}
{
	if(key)
		seek(*this, key, opts);
}
#pragma GCC diagnostic pop

static std::vector<rocksdb::Iterator *>
ircd::db::_make_iterators(database &d,
                          database::column *const *const &column,
                          const size_t &column_count,
                          const rocksdb::ReadOptions &opts)
{
	using rocksdb::Iterator;
	using rocksdb::ColumnFamilyHandle;
	assert(column_count <= d.columns.size());

	//const ctx::critical_assertion ca;
	// NewIterators() has been seen to lead to IO and block the ircd::ctx;
	// specifically when background options are aggressive and shortly
	// after db opens. It would be nice if we could maintain the
	// critical_assertion for this function, as we could eliminate the
	// vector allocation for ColumnFamilyHandle pointers.

	std::vector<ColumnFamilyHandle *> handles(column_count);
	std::transform(column, column + column_count, begin(handles), []
	(database::column *const &ptr)
	{
		assert(ptr);
		return ptr->handle.get();
	});

	std::vector<Iterator *> ret;
	const ctx::stack_usage_assertion sua;
	throw_on_error
	{
		d.d->NewIterators(opts, handles, &ret)
	};

	return ret;
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
		throw not_found
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
		throw not_found
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
ircd::db::row::cached()
const
{
	return std::all_of(std::begin(*this), std::end(*this), []
	(const auto &cell)
	{
		db::column &column(mutable_cast(cell));
		return cell.valid() && db::cached(column, cell.key());
	});
}

bool
ircd::db::row::cached(const string_view &key)
const
{
	return std::all_of(std::begin(*this), std::end(*this), [&key]
	(const auto &cell)
	{
		db::column &column(mutable_cast(cell));
		return db::cached(column, key);
	});
}

bool
ircd::db::row::valid_all(const string_view &s)
const
{
	return !empty() && std::all_of(std::begin(*this), std::end(*this), [&s]
	(const auto &cell)
	{
		return cell.valid(s);
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

bool
ircd::db::row::valid_all()
const
{
	return !empty() && std::all_of(std::begin(*this), std::end(*this), []
	(const auto &cell)
	{
		return cell.valid();
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
                     const gopts &opts)
:cell
{
	column(d[colname]), std::unique_ptr<rocksdb::Iterator>{}, opts
}
{
}

ircd::db::cell::cell(database &d,
                     const string_view &colname,
                     const string_view &index,
                     const gopts &opts)
:cell
{
	column(d[colname]), index, opts
}
{
}

ircd::db::cell::cell(column column,
                     const string_view &index,
                     const gopts &opts)
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
                     const gopts &opts)
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
                     const gopts &opts)
:c{std::move(column)}
,ss{opts.snapshot}
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
	const auto _opts
	{
		make_opts(opts)
	};

	if(!seek(c, index, _opts, this->it))
		return false;

	return valid(index);
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
ircd::db::cell::valid(const string_view &s)
const
{
	return valid() && db::valid_eq(*it, s);
}

bool
ircd::db::cell::valid_gt(const string_view &s)
const
{
	return valid() && db::valid_gt(*it, s);
}

bool
ircd::db::cell::valid_lte(const string_view &s)
const
{
	return valid() && db::valid_lte(*it, s);
}

bool
ircd::db::cell::valid()
const
{
	return bool(it) && db::valid(*it);
}

///////////////////////////////////////////////////////////////////////////////
//
// db/domain.h
//

const ircd::db::gopts
ircd::db::domain::applied_opts
{
	get::PREFIX
};

bool
ircd::db::seek(domain::const_iterator_base &it,
               const pos &p)
{
	switch(p)
	{
		case pos::BACK:
		{
			// This is inefficient as per RocksDB's prefix impl. unknown why
			// a seek to NEXT is still needed after walking back one.
			while(seek(it, pos::NEXT));
			if(seek(it, pos::PREV))
				seek(it, pos::NEXT);

			return bool(it);
		}

		default:
			break;
	}

	it.opts |= domain::applied_opts;
	return seek(static_cast<column::const_iterator_base &>(it), p);
}

bool
ircd::db::seek(domain::const_iterator_base &it,
               const string_view &p)
{
	it.opts |= domain::applied_opts;
	return seek(static_cast<column::const_iterator_base &>(it), p);
}

ircd::db::domain::const_iterator
ircd::db::domain::begin(const string_view &key,
                        gopts opts)
{
	const_iterator ret
	{
		c, {}, std::move(opts)
	};

	seek(ret, key);
	return ret;
}

ircd::db::domain::const_iterator
ircd::db::domain::end(const string_view &key,
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
/// iterations of a domain should only be used for debugging and statistics! The
/// domain should be ordered the way it will be primarily accessed using the
/// comparator. If it will be accessed in different directions, make another
/// domain column.
ircd::db::domain::const_reverse_iterator
ircd::db::domain::rbegin(const string_view &key,
                         gopts opts)
{
	const_reverse_iterator ret
	{
		c, {}, std::move(opts)
	};

	if(seek(ret, key))
		seek(ret, pos::BACK);

	return ret;
}

ircd::db::domain::const_reverse_iterator
ircd::db::domain::rend(const string_view &key,
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

ircd::db::domain::const_iterator &
ircd::db::domain::const_iterator::operator--()
{
	if(likely(bool(*this)))
		seek(*this, pos::PREV);
	else
		seek(*this, pos::BACK);

	return *this;
}

ircd::db::domain::const_iterator &
ircd::db::domain::const_iterator::operator++()
{
	if(likely(bool(*this)))
		seek(*this, pos::NEXT);
	else
		seek(*this, pos::FRONT);

	return *this;
}

ircd::db::domain::const_reverse_iterator &
ircd::db::domain::const_reverse_iterator::operator--()
{
	if(likely(bool(*this)))
		seek(*this, pos::NEXT);
	else
		seek(*this, pos::FRONT);

	return *this;
}

ircd::db::domain::const_reverse_iterator &
ircd::db::domain::const_reverse_iterator::operator++()
{
	if(likely(bool(*this)))
		seek(*this, pos::PREV);
	else
		seek(*this, pos::BACK);

	return *this;
}

const ircd::db::domain::const_iterator_base::value_type &
ircd::db::domain::const_iterator_base::operator*()
const
{
	const auto &prefix
	{
		describe(*c).prefix
	};

	// Fetch the full value like a standard column first
	column::const_iterator_base::operator*();
	string_view &key{val.first};

	// When there's no prefixing this domain column is just
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

///////////////////////////////////////////////////////////////////////////////
//
// db/column.h
//

void
ircd::db::drop(column &column)
{
	database::column &c(column);
	drop(c);
}

void
ircd::db::check(column &column)
{
	database &d(column);
	const auto &files
	{
		db::files(column)
	};

	for(const auto &file : files)
	{
		const auto &path
		{
			 // remove false leading slash; the rest is relative to db.
			lstrip(file, '/')
		};

		db::check(d, path);
	}
}

void
ircd::db::sort(column &column,
               const bool &blocking,
               const bool &now)
{
	database::column &c(column);
	database &d(*c.d);

	rocksdb::FlushOptions opts;
	opts.wait = blocking;
	opts.allow_write_stall = now;

	const ctx::uninterruptible::nothrow ui;
	const std::lock_guard lock
	{
		d.write_mutex
	};

	log::debug
	{
		log, "[%s]'%s' @%lu FLUSH (sort) %s %s",
		name(d),
		name(c),
		sequence(d),
		blocking? "blocking"_sv: "non-blocking"_sv,
		now? "now"_sv: "later"_sv
	};

	throw_on_error
	{
		d.d->Flush(opts, c)
	};
}

void
ircd::db::compact(column &column,
                  const std::pair<int, int> &level,
                  const compactor &cb)
{
	database::column &c(column);
	database &d(*c.d);

	const auto &dst_level{level.second};
	const auto &src_level{level.first};

	rocksdb::ColumnFamilyMetaData cfmd;
	d.d->GetColumnFamilyMetaData(c, &cfmd);
	for(const auto &level : cfmd.levels)
	{
		if(src_level != -1 && src_level != level.level)
			continue;

		if(level.files.empty())
			continue;

		const ctx::uninterruptible ui;
		const std::lock_guard lock
		{
			d.write_mutex
		};

		const auto &to_level
		{
			dst_level > -1? dst_level : level.level
		};

		rocksdb::CompactionOptions opts;
		opts.output_file_size_limit = 1_GiB; //TODO: conf

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
			log, "[%s]'%s' COMPACT L%d -> L%d files:%zu size:%zu",
			name(d),
			name(c),
			level.level,
			to_level,
			level.files.size(),
			level.size
		};

		throw_on_error
		{
			d.d->CompactFiles(opts, c, files, to_level)
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
	const ctx::uninterruptible ui;

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
	opts.exclusive_manual_compaction = true;
	opts.allow_write_stall = true;
	opts.change_level = true;
	opts.target_level = std::max(to_level, -1);
	opts.bottommost_level_compaction = rocksdb::BottommostLevelCompaction::kForce;

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
		log, "[%s]'%s' @%lu COMPACT [%s, %s] -> L:%d (Lmax:%d Lbase:%d)",
		name(d),
		name(c),
		sequence(d),
		range.first,
		range.second,
		opts.target_level,
		d.d->NumberLevels(c),
		d.d->MaxMemCompactionLevel(c),
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

	const std::lock_guard lock{d.write_mutex};
	const ctx::uninterruptible::nothrow ui;
	throw_on_error
	{
		d.d->IngestExternalFile(c, files, opts)
	};
}

void
ircd::db::del(column &column,
              const std::pair<string_view, string_view> &range,
              const sopts &sopts)
{
	database &d(column);
	database::column &c(column);
	auto opts(make_opts(sopts));

	const std::lock_guard lock{d.write_mutex};
	const ctx::uninterruptible::nothrow ui;
	const ctx::stack_usage_assertion sua;
	log::debug
	{
		log, "'%s' %lu '%s' RANGE DELETE",
		name(d),
		sequence(d),
		name(c),
	};

	throw_on_error
	{
		d.d->DeleteRange(opts, c, slice(range.first), slice(range.second))
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

	const std::lock_guard lock{d.write_mutex};
	const ctx::uninterruptible::nothrow ui;
	const ctx::stack_usage_assertion sua;
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

	const std::lock_guard lock{d.write_mutex};
	const ctx::uninterruptible::nothrow ui;
	const ctx::stack_usage_assertion sua;
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

uint64_t
ircd::db::read(column &column,
               const keys &keys,
               const bufs &bufs,
               const gopts &opts)
{
	const columns columns
	{
		&column, 1
	};

	return read(columns, keys, bufs, opts);
}

uint64_t
ircd::db::read(const columns &c,
               const keys &key,
               const bufs &buf,
               const gopts &gopts)
{
	if(c.empty())
		return 0UL;

	const auto &num
	{
		key.size()
	};

	if(unlikely(!num || num > 64 || num > buf.size()))
		throw std::out_of_range
		{
			"db::read() :too many columns or vector size mismatch"
		};

	_read_op op[num];
	for(size_t i(0); i < num; ++i)
		op[i] =
		{
			c[std::min(c.size() - 1, i)], key[i]
		};

	uint64_t i(0), ret(0);
	auto opts(make_opts(gopts));
	_read({op, num}, opts, [&i, &ret, &buf]
	(column &, const column::delta &d, const rocksdb::Status &s)
	{
		const auto &val
		{
			std::get<column::delta::VAL>(d)
		};

		buf[i] = mutable_buffer
		{
			buf[i], size(val)
		};

		const auto copied
		{
			copy(buf[i], val)
		};

		ret |= (uint64_t(s.ok()) << i++);
		return true;
	});

	return ret;
}

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

size_t
ircd::db::bytes_value(column &column,
                      const string_view &key,
                      const gopts &gopts)
{
	size_t ret{0};
	column(key, std::nothrow, gopts, [&ret]
	(const string_view &value)
	{
		ret = value.size();
	});

	return ret;
}

size_t
ircd::db::bytes(column &column,
                const std::pair<string_view, string_view> &key,
                const gopts &gopts)
{
	database &d(column);
	database::column &c(column);
	const rocksdb::Range range[1]
	{
		{ slice(key.first), slice(key.second) }
	};

	uint64_t ret[1] {0};
	d.d->GetApproximateSizes(c, range, 1, ret);
	return ret[0];
}

bool
ircd::db::has(column &column,
              const string_view &key,
              const gopts &gopts)
{
	database &d(column);
	database::column &c(column);

	// Perform a co-RP query to the filtration
	//
	// NOTE disabled for rocksdb >= v5.15 due to a regression
	// where rocksdb does not init SuperVersion data in the column
	// family handle and this codepath triggers null derefs and ub.
	//
	// NOTE works on rocksdb 6.6.4 but unconditionally copies value.
	auto opts(make_opts(gopts));
	if(c.table_opts.filter_policy && (false))
	{
		auto opts(make_opts(gopts));
		const scope_restore read_tier
		{
			opts.read_tier, NON_BLOCKING
		};

		const scope_restore fill_cache
		{
			opts.fill_cache, false
		};

		std::string discard;
		bool value_found {false};
		const bool key_may_exist
		{
			d.d->KeyMayExist(opts, c, slice(key), &discard, &value_found)
		};

		if(!key_may_exist)
			return false;

		if(value_found)
			return true;
	}

	std::unique_ptr<rocksdb::Iterator> it;
	if(!seek(c, key, opts, it))
		return false;

	assert(bool(it));
	return valid_eq(*it, key);
}

uint64_t
ircd::db::has(column &column,
              const keys &key,
              const gopts &opts)
{
	const columns columns
	{
		&column, 1
	};

	return has(columns, key, opts);
}

uint64_t
ircd::db::has(const columns &c,
              const keys &key,
              const gopts &gopts)
{
	if(c.empty())
		return 0UL;

	const auto &num
	{
		key.size()
	};

	if(unlikely(!num || num > 64))
		throw std::out_of_range
		{
			"db::has() :too many columns or vector size mismatch"
		};

	_read_op op[num];
	for(size_t i(0); i < num; ++i)
		op[i] =
		{
			c[std::min(c.size() - 1, i)], key[i]
		};

	uint64_t i(0), ret(0);
	auto opts(make_opts(gopts));
	_read({op, num}, opts, [&i, &ret, &opts]
	(column &, const column::delta &, const rocksdb::Status &s)
	{
		uint64_t found {0};
		found |= s.ok();
		found |= s.IsIncomplete() & (opts.read_tier == NON_BLOCKING);
		ret |= (found << i++);
		return true;
	});

	return ret;
}

bool
ircd::db::prefetch(column &column,
                   const string_view &key,
                   const gopts &gopts)
{
	static construction instance
	{
		[] { prefetcher = new struct prefetcher(); }
	};

	assert(prefetcher);
	return (*prefetcher)(column, key, gopts);
}

#if 0
bool
ircd::db::cached(column &column,
                 const string_view &key,
                 const gopts &gopts)
{
	return exists(cache(column), key);
}
#endif

bool
ircd::db::cached(column &column,
                 const string_view &key,
                 const gopts &gopts)
{
	using rocksdb::Status;

	auto opts(make_opts(gopts));
	opts.read_tier = NON_BLOCKING;
	opts.fill_cache = false;

	std::unique_ptr<rocksdb::Iterator> it;
	database::column &c(column);
	const bool valid
	{
		seek(c, key, opts, it)
	};

	assert(it);
	const auto code
	{
		it->status().code()
	};

	return false
	|| (valid && valid_eq(*it, key))
	|| (!valid && code != rocksdb::Status::kIncomplete)
	;
}

[[gnu::hot]]
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

[[gnu::hot]]
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
	database::column &c(mutable_cast(column));
	database &d(mutable_cast(column));
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
	database::column &c(mutable_cast(column));
	database &d(mutable_cast(column));
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
	database::column &c(mutable_cast(column));
	database &d(mutable_cast(column));
	if(!d.d->GetMapProperty(c, slice(name), &ret))
		ret.emplace(std::string{name}, property<std::string>(column, name));

	return ret;
}

ircd::db::options
ircd::db::getopt(const column &column)
{
	database &d(mutable_cast(column));
	database::column &c(mutable_cast(column));
	return options
	{
		static_cast<rocksdb::ColumnFamilyOptions>(d.d->GetOptions(c))
	};
}

size_t
ircd::db::bytes(const column &column)
{
	rocksdb::ColumnFamilyMetaData cfm;
	database &d(mutable_cast(column));
	database::column &c(mutable_cast(column));
	assert(bool(c.handle));
	d.d->GetColumnFamilyMetaData(c.handle.get(), &cfm);
	return cfm.size;
}

size_t
ircd::db::file_count(const column &column)
{
	rocksdb::ColumnFamilyMetaData cfm;
	database &d(mutable_cast(column));
	database::column &c(mutable_cast(column));
	assert(bool(c.handle));
	d.d->GetColumnFamilyMetaData(c.handle.get(), &cfm);
	return cfm.file_count;
}

std::vector<std::string>
ircd::db::files(const column &column)
{
	database::column &c(mutable_cast(column));
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

[[gnu::hot]]
const ircd::db::descriptor &
ircd::db::describe(const column &column)
noexcept
{
	const database::column &c(column);
	return describe(c);
}

[[gnu::hot]]
const std::string &
ircd::db::name(const column &column)
noexcept
{
	const database::column &c(column);
	return name(c);
}

[[gnu::hot]]
uint32_t
ircd::db::id(const column &column)
noexcept
{
	const database::column &c(column);
	return id(c);
}

//
// column
//

ircd::db::column::column(database &d,
                         const string_view &column_name)
:column
{
	d[column_name]
}
{
}

ircd::db::column::column(database &d,
                         const string_view &column_name,
                         const std::nothrow_t)
:c{[&d, &column_name]
{
	const int32_t cfid
	{
		d.cfid(std::nothrow, column_name)
	};

	return cfid >= 0?
		&d[cfid]:
		nullptr;
}()}
{
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

[[gnu::hot]]
ircd::db::column::operator
bool()
const noexcept
{
	return c && !dropped(*c);
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
ircd::db::column::last(gopts gopts)
{
	const_iterator ret
	{
		c, {}, std::move(gopts)
	};

	seek(ret, pos::BACK);
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
noexcept
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
noexcept
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

ircd::db::column::const_iterator_base::operator bool()
const noexcept
{
	if(!it)
		return false;

	if(!valid(*it))
		return false;

	return true;
}

bool
ircd::db::operator!=(const column::const_iterator_base &a, const column::const_iterator_base &b)
noexcept
{
	const uint operands
	{
		uint(bool(a)) +
		uint(bool(b))
	};

	// Two invalid iterators are equal; one invalid iterator is not.
	if(likely(operands <= 1))
		return operands == 1;

	// Two valid iterators are compared
	assert(operands == 2);
	const auto &ak(a.it->key());
	const auto &bk(b.it->key());
	return ak.compare(bk) != 0;
}

bool
ircd::db::operator==(const column::const_iterator_base &a, const column::const_iterator_base &b)
noexcept
{
	const uint operands
	{
		uint(bool(a)) +
		uint(bool(b))
	};

	// Two valid iterators are compared
	if(likely(operands > 1))
	{
		const auto &ak(a.it->key());
		const auto &bk(b.it->key());
		return ak.compare(bk) == 0;
	}

	// Two invalid iterators are equal; one invalid iterator is not.
	return operands == 0;
}

bool
ircd::db::operator>(const column::const_iterator_base &a, const column::const_iterator_base &b)
noexcept
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
noexcept
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
	return seek(c, p, make_opts(it.opts), it.it);
}
template bool ircd::db::seek<ircd::db::pos>(column::const_iterator_base &, const pos &);
template bool ircd::db::seek<ircd::string_view>(column::const_iterator_base &, const string_view &);

///////////////////////////////////////////////////////////////////////////////
//
// opts.h
//

//
// options
//

ircd::db::options::options(const database &d)
:options{d.d->GetDBOptions()}
{
}

ircd::db::options::options(const database::column &c)
:options
{
	rocksdb::ColumnFamilyOptions
	{
		c.d->d->GetOptions(c.handle.get())
	}
}{}

ircd::db::options::options(const rocksdb::DBOptions &opts)
{
	throw_on_error
	{
		rocksdb::GetStringFromDBOptions(this, opts)
	};
}

ircd::db::options::options(const rocksdb::ColumnFamilyOptions &opts)
{
	throw_on_error
	{
		rocksdb::GetStringFromColumnFamilyOptions(this, opts)
	};
}

ircd::db::options::operator rocksdb::PlainTableOptions()
const
{
	rocksdb::PlainTableOptions ret;
	throw_on_error
	{
		rocksdb::GetPlainTableOptionsFromString(ret, *this, &ret)
	};

	return ret;
}

ircd::db::options::operator rocksdb::BlockBasedTableOptions()
const
{
	rocksdb::BlockBasedTableOptions ret;
	throw_on_error
	{
		rocksdb::GetBlockBasedTableOptionsFromString(ret, *this, &ret)
	};

	return ret;
}

ircd::db::options::operator rocksdb::ColumnFamilyOptions()
const
{
	rocksdb::ColumnFamilyOptions ret;
	throw_on_error
	{
		rocksdb::GetColumnFamilyOptionsFromString(ret, *this, &ret)
	};

	return ret;
}

ircd::db::options::operator rocksdb::DBOptions()
const
{
	rocksdb::DBOptions ret;
	throw_on_error
	{
		rocksdb::GetDBOptionsFromString(ret, *this, &ret)
	};

	return ret;
}

ircd::db::options::operator rocksdb::Options()
const
{
	rocksdb::Options ret;
	throw_on_error
	{
		rocksdb::GetOptionsFromString(ret, *this, &ret)
	};

	return ret;
}

//
// options::map
//

ircd::db::options::map::map(const options &o)
{
	throw_on_error
	{
		rocksdb::StringToMap(o, this)
	};
}

ircd::db::options::map::operator rocksdb::PlainTableOptions()
const
{
	rocksdb::PlainTableOptions ret;
	throw_on_error
	{
		rocksdb::GetPlainTableOptionsFromMap(ret, *this, &ret)
	};

	return ret;
}

ircd::db::options::map::operator rocksdb::BlockBasedTableOptions()
const
{
	rocksdb::BlockBasedTableOptions ret;
	throw_on_error
	{
		rocksdb::GetBlockBasedTableOptionsFromMap(ret, *this, &ret)
	};

	return ret;
}

ircd::db::options::map::operator rocksdb::ColumnFamilyOptions()
const
{
	rocksdb::ColumnFamilyOptions ret;
	throw_on_error
	{
		rocksdb::GetColumnFamilyOptionsFromMap(ret, *this, &ret)
	};

	return ret;
}

ircd::db::options::map::operator rocksdb::DBOptions()
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
// cache.h
//

void
ircd::db::clear(rocksdb::Cache &cache)
{
	cache.EraseUnRefEntries();
}

bool
ircd::db::remove(rocksdb::Cache &cache,
                 const string_view &key)
{
	cache.Erase(slice(key));
	return true;
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
ircd::db::insert(rocksdb::Cache &cache,
                 const string_view &key,
                 unique_buffer<const_buffer> &&value)
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
		             mutable_cast(data(value.release())),
		             value_size,
		             deleter,
		             nullptr)
	};

	return true;
}

void
ircd::db::for_each(const rocksdb::Cache &cache,
                   const cache_closure &closure)
{
	// Due to the use of the global variables which are required when using a
	// C-style callback for RocksDB, we have to make use of this function
	// exclusive for different contexts.
	thread_local ctx::mutex mutex;
	const std::lock_guard lock{mutex};

	thread_local rocksdb::Cache *_cache;
	_cache = mutable_cast(&cache);

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

#ifdef IRCD_DB_HAS_CACHE_GETCHARGE
size_t
ircd::db::charge(const rocksdb::Cache &cache_,
                 const string_view &key)
{
	auto &cache
	{
		mutable_cast(cache_)
	};

	const custom_ptr<rocksdb::Cache::Handle> handle
	{
		cache.Lookup(slice(key)), [&cache](auto *const &handle)
		{
			cache.Release(handle);
		}
	};

	return cache.GetCharge(handle);
}
#else
size_t
ircd::db::charge(const rocksdb::Cache &cache,
                 const string_view &key)
{
	return 0UL;
}
#endif

[[gnu::hot]]
bool
ircd::db::exists(const rocksdb::Cache &cache_,
                 const string_view &key)
{
	auto &cache
	{
		mutable_cast(cache_)
	};

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
ircd::db::count(const rocksdb::Cache &cache)
{
	size_t ret(0);
	for_each(cache, [&ret]
	(const const_buffer &)
	{
		++ret;
	});

	return ret;
}

size_t
ircd::db::pinned(const rocksdb::Cache &cache)
{
	return cache.GetPinnedUsage();
}

size_t
ircd::db::usage(const rocksdb::Cache &cache)
{
	return cache.GetUsage();
}

void
ircd::db::capacity(rocksdb::Cache &cache,
                   const size_t &cap)
{
	cache.SetCapacity(cap);
}

size_t
ircd::db::capacity(const rocksdb::Cache &cache)
{
	return cache.GetCapacity();
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
// error.h
//

//
// error::not_found
//

decltype(ircd::db::error::not_found::_not_found_)
ircd::db::error::not_found::_not_found_
{
	rocksdb::Status::NotFound()
};

//
// error::not_found::not_found
//

ircd::db::error::not_found::not_found()
:error
{
	generate_skip, _not_found_
}
{
	strlcpy(buf, "NotFound");
}

//
// error
//

decltype(ircd::db::error::_no_code_)
ircd::db::error::_no_code_
{
	rocksdb::Status::OK()
};

//
// error::error
//

ircd::db::error::error(internal_t,
                       const rocksdb::Status &s,
                       const string_view &fmt,
                       const va_rtti &ap)
:error
{
	s
}
{
	const string_view &msg{buf};
	const mutable_buffer remain
	{
		buf + size(msg), sizeof(buf) - size(msg)
	};

	fmt::vsprintf
	{
		remain, fmt, ap
	};
}

ircd::db::error::error(const rocksdb::Status &s)
:error
{
	generate_skip, s
}
{
	fmt::sprintf
	{
		buf, "(%u:%u:%u) %s %s :%s",
		this->code,
		this->subcode,
		this->severity,
		reflect(rocksdb::Status::Severity(this->severity)),
		reflect(rocksdb::Status::Code(this->code)),
		s.getState(),
	};
}

ircd::db::error::error(generate_skip_t,
                       const rocksdb::Status &s)
:ircd::error
{
	generate_skip
}
,code
{
	s.code()
}
,subcode
{
	s.subcode()
}
,severity
{
	s.severity()?
		s.severity():

	code == rocksdb::Status::kCorruption?
		rocksdb::Status::kHardError:

	rocksdb::Status::kNoError
}
{
}

///////////////////////////////////////////////////////////////////////////////
//
// merge.h
//

std::string
__attribute__((noreturn))
ircd::db::merge_operator(const string_view &key,
                         const std::pair<string_view, string_view> &delta)
{
	//ircd::json::index index{delta.first};
	//index += delta.second;
	//return index;

	throw ircd::not_implemented
	{
		"db::merge_operator()"
	};
}

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
noexcept
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

///////////////////////////////////////////////////////////////////////////////
//
// delta.h
//

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

///////////////////////////////////////////////////////////////////////////////
//
// db.h (internal)
//

//
// throw_on_error
//

ircd::db::throw_on_error::throw_on_error(const rocksdb::Status &status)
{
	using rocksdb::Status;

	switch(status.code())
	{
		case Status::kOk:
			return;

		case Status::kNotFound:
			throw not_found{};

		#ifdef RB_DEBUG
		//case Status::kCorruption:
		case Status::kNotSupported:
		case Status::kInvalidArgument:
			debugtrap();
			[[fallthrough]];
		#endif

		default:
			throw error
			{
				status
			};
	}
}

//
// error_to_status
//

ircd::db::error_to_status::error_to_status(const std::exception &e)
:rocksdb::Status
{
	Status::Aborted(slice(string_view(e.what())))
}
{
}

ircd::db::error_to_status::error_to_status(const std::system_error &e)
:error_to_status{e.code()}
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
		{
			const auto &message(e.message());
			return Status::Aborted(slice(string_view(message)));
		}
	}
}()}
{
}

//
// writebatch suite
//

void
ircd::db::append(rocksdb::WriteBatch &batch,
                 const cell::delta &delta)
{
	auto &column
	{
		std::get<cell *>(delta)->c
	};

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
	if(unlikely(!column))
	{
		// Note: Unknown at this time whether allowing attempts at writing
		// to a null column should be erroneous or silently ignored. It's
		// highly likely this log message will be removed soon to allow
		// toggling database columns for optimization without touching calls.
		log::critical
		{
			log, "Attempting to transact a delta for a null column"
		};

		return;
	}

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
	#ifdef RB_DEBUG
	ircd::timer timer;
	#endif

	const std::lock_guard lock{d.write_mutex};
	const ctx::uninterruptible ui;
	const ctx::stack_usage_assertion sua;
	throw_on_error
	{
		d.d->Write(opts, &batch)
	};

	#ifdef RB_DEBUG
	char dbuf[192];
	log::debug
	{
		log, "[%s] %lu COMMIT %s in %ld$us",
		d.name,
		sequence(d),
		debug(dbuf, batch),
		timer.at<microseconds>().count()
	};
	#endif
}

ircd::string_view
ircd::db::debug(const mutable_buffer &buf,
                const rocksdb::WriteBatch &batch)
{
	char pbuf[48] {0};
	const auto len(snprintf
	(
		data(buf), size(buf),
		"%d deltas; %s %s+%s+%s+%s+%s+%s+%s+%s+%s"
		,batch.Count()
		,pretty(pbuf, iec(batch.GetDataSize())).data()
		,batch.HasPut()? "PUT": ""
		,batch.HasDelete()? "DEL": ""
		,batch.HasSingleDelete()? "SDL": ""
		,batch.HasDeleteRange()? "DRG": ""
		,batch.HasMerge()? "MRG": ""
		,batch.HasBeginPrepare()? "BEG": ""
		,batch.HasEndPrepare()? "END": ""
		,batch.HasCommit()? "COM-": ""
		,batch.HasRollback()? "RB^": ""
	));

	return string_view
	{
		data(buf), len
	};
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

//
// read suite
//

namespace ircd::db
{
	static rocksdb::Status _seek(database::column &, rocksdb::PinnableSlice &, const string_view &, const rocksdb::ReadOptions &);
}

rocksdb::Status
ircd::db::_read(column &column,
                const string_view &key,
                const rocksdb::ReadOptions &opts,
                const column::view_closure &closure)
{
	std::string buf;
	rocksdb::PinnableSlice ps
	{
		&buf
	};

	database::column &c(column);
	const rocksdb::Status ret
	{
		_seek(c, ps, key, opts)
	};

	if(!valid(ret))
		return ret;

	const string_view value
	{
		slice(ps)
	};

	if(likely(closure))
		closure(value);

	// Update stats about whether the pinnable slices we obtained have internal
	// copies or referencing the cache copy.
	database &d(column);
	c.stats->get_referenced += buf.empty();
	d.stats->get_referenced += buf.empty();
	c.stats->get_copied += !buf.empty();
	d.stats->get_copied += !buf.empty();
	return ret;
}

rocksdb::Status
ircd::db::_seek(database::column &c,
                rocksdb::PinnableSlice &s,
                const string_view &key,
                const rocksdb::ReadOptions &ropts)
{
	const ctx::uninterruptible::nothrow ui;
	const ctx::stack_usage_assertion sua;

	rocksdb::ColumnFamilyHandle *const &cf(c);
	database &d(*c.d);

	#ifdef RB_DEBUG_DB_SEEK
	const ircd::timer timer;
	#endif

	const rocksdb::Status ret
	{
		d.d->Get(ropts, cf, slice(key), &s)
	};

	#ifdef RB_DEBUG_DB_SEEK
	log::debug
	{
		log, "[%s] %lu:%lu SEEK %s in %ld$us '%s'",
		name(d),
		sequence(d),
		sequence(ropts.snapshot),
		ret.ToString(),
		timer.at<microseconds>().count(),
		name(c)
	};
	#endif

	return ret;
}

//
// parallel read suite
//

namespace ircd::db
{
	static void _seek(const vector_view<_read_op> &, const vector_view<rocksdb::Status> &, const vector_view<rocksdb::PinnableSlice> &, const rocksdb::ReadOptions &);
}

bool
ircd::db::_read(const vector_view<_read_op> &op,
                const rocksdb::ReadOptions &ropts,
                const _read_closure &closure)
{
	assert(op.size() >= 1);
	assert(op.size() <= IOV_MAX);
	const size_t &num
	{
		op.size()
	};

	std::string buf[num];
	rocksdb::PinnableSlice val[num];
	for(size_t i(0); i < num; ++i)
		new (val + i) rocksdb::PinnableSlice
		{
			buf + i
		};

	const bool parallelize
	{
		#ifdef IRCD_DB_HAS_MULTIGET_DIRECT
			true && num > 1
		#else
			false
		#endif
	};

	rocksdb::Status status[num];
	if(!parallelize)
		for(size_t i(0); i < num; ++i)
		{
			database::column &column(std::get<column>(op[i]));
			status[i] = _seek(column, val[i], std::get<1>(op[i]), ropts);
		}
	else
		_seek(op, {status, num}, {val, num}, ropts);

	bool ret(true);
	if(closure)
		for(size_t i(0); i < num && ret; ++i)
		{
			const column::delta delta(std::get<1>(op[i]), slice(val[i]));
			ret = closure(std::get<column>(op[i]), delta, status[i]);
		}

	// Update stats about whether the pinnable slices we obtained have internal
	// copies or referencing the cache copy.
	for(size_t i(0); i < num; ++i)
	{
		database &d(std::get<column>(op[i]));
		database::column &c(std::get<column>(op[i]));

		// Find the correct stats to update, one for the specific column and
		// one for the database total.
		ircd::stats::item<uint64_t> *item_[2]
		{
			parallelize && buf[i].empty()?    &c.stats->multiget_referenced:
			parallelize?                      &c.stats->multiget_copied:
			buf[i].empty()?                   &c.stats->get_referenced:
			                                  &c.stats->get_copied,

			parallelize && buf[i].empty()?    &d.stats->multiget_referenced:
			parallelize?                      &d.stats->multiget_copied:
			buf[i].empty()?                   &d.stats->get_referenced:
			                                  &d.stats->get_copied,
		};

		for(auto *const &item : item_)
			++(*item);
	}

	return ret;
}

void
ircd::db::_seek(const vector_view<_read_op> &op,
                const vector_view<rocksdb::Status> &ret,
                const vector_view<rocksdb::PinnableSlice> &val,
                const rocksdb::ReadOptions &ropts)
{
	assert(ret.size() == op.size());
	assert(ret.size() == val.size());

	const ctx::stack_usage_assertion sua;
	const ctx::uninterruptible::nothrow ui;

	assert(op.size() >= 1);
	database &d(std::get<0>(op[0]));
	const size_t &num
	{
		op.size()
	};

	rocksdb::Slice key[num];
	std::transform(begin(op), end(op), key, []
	(const auto &op)
	{
		return slice(std::get<1>(op));
	});

	rocksdb::ColumnFamilyHandle *cf[num];
	std::transform(begin(op), end(op), cf, []
	(auto &op_)
	{
		auto &op(mutable_cast(op_));
		database::column &c(std::get<column>(op));
		return static_cast<rocksdb::ColumnFamilyHandle *>(c);
	});

	#ifdef RB_DEBUG_DB_SEEK
	const ircd::timer timer;
	#endif

	#ifdef IRCD_DB_HAS_MULTIGET_BATCHED
	d.d->MultiGet(ropts, num, cf, key, val.data(), ret.data());
	#endif

	#ifdef RB_DEBUG_DB_SEEK
	log::debug
	{
		log, "[%s] %lu:%lu SEEK parallel:%zu ok:%zu nf:%zu inc:%zu in %ld$us",
		name(d),
		sequence(d),
		sequence(ropts.snapshot),
		ret.size(),
		std::count_if(begin(ret), end(ret), [](auto&& s) { return s.ok(); }),
		std::count_if(begin(ret), end(ret), [](auto&& s) { return s.IsNotFound(); }),
		std::count_if(begin(ret), end(ret), [](auto&& s) { return s.IsIncomplete(); }),
		timer.at<microseconds>().count(),
	};
	#endif
}

//
// iterator seek suite
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
	seek(c, key, make_opts(opts), ret);
	return ret;
}

template<class pos>
bool
ircd::db::seek(database::column &c,
               const pos &p,
               const rocksdb::ReadOptions &opts,
               std::unique_ptr<rocksdb::Iterator> &it)
{
	if(!it)
	{
		const ctx::uninterruptible::nothrow ui;

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
try
{
	const ctx::uninterruptible ui;

	#ifdef RB_DEBUG_DB_SEEK
	database &d(*c.d);
	const ircd::timer timer;
	#endif

	_seek_(it, p);

	#ifdef RB_DEBUG_DB_SEEK
	log::debug
	{
		log, "[%s] %lu:%lu SEEK %s %s in %ld$us '%s'",
		name(d),
		sequence(d),
		sequence(opts.snapshot),
		valid(it)? "VALID" : "INVALID",
		it.status().ToString(),
		timer.at<microseconds>().count(),
		name(c)
	};
	#endif

	return valid(it);
}
catch(const error &e)
{
	const database &d(*c.d);
	log::critical
	{
		log, "[%s][%s] %lu:%lu SEEK key :%s",
		name(d),
		name(c),
		sequence(d),
		sequence(opts.snapshot),
		e.what(),
	};

	throw;
}

bool
ircd::db::_seek(database::column &c,
                const pos &p,
                const rocksdb::ReadOptions &opts,
                rocksdb::Iterator &it)
try
{
	const ctx::stack_usage_assertion sua;

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
		log, "[%s] %lu:%lu SEEK[%s] %s -> %s in %ld$us '%s'",
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
catch(const error &e)
{
	const database &d(*c.d);
	log::critical
	{
		log, "[%s][%s] %lu:%lu SEEK %s %s :%s",
		name(d),
		name(c),
		sequence(d),
		sequence(opts.snapshot),
		reflect(p),
		it.Valid()? "VALID" : "INVALID",
		e.what(),
	};

	throw;
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

//
// validation suite
//

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
	return valid(it) && proffer(it);
}

bool
ircd::db::valid(const rocksdb::Iterator &it)
{
	if(likely(it.Valid()))
		return true;

	switch(it.status().code())
	{
		using rocksdb::Status;

		case Status::kOk:
		case Status::kNotFound:
		case Status::kIncomplete:
			return it.Valid();

		default:
			throw_on_error
			{
				it.status()
			};

			__builtin_unreachable();
	}
}

bool
ircd::db::valid(const rocksdb::Status &s)
{
	switch(s.code())
	{
		using rocksdb::Status;

		case Status::kOk:
			return true;

		case Status::kNotFound:
		case Status::kIncomplete:
			return false;

		default:
			throw_on_error{s};
			__builtin_unreachable();
	}
}

//
// column_names
//

std::vector<std::string>
ircd::db::column_names(const std::string &path,
                       const std::string &options)
{
	const rocksdb::DBOptions opts
	{
		db::options(options)
	};

	return column_names(path, opts);
}

/// Note that if there is no database found at path we still return a
/// vector containing the column name "default". This function is not
/// to be used as a test for whether the database exists. It returns
/// the columns required to be described at `path`. That will always
/// include the default column (RocksDB sez) even if database doesn't
/// exist yet.
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
catch(const not_found &)
{
	return // No database found at path.
	{
		{ rocksdb::kDefaultColumnFamilyName }
	};
}

//
// Misc
//

namespace ircd::db
{
	extern conf::item<std::string> compression_default;
}

decltype(ircd::db::compression_default)
ircd::db::compression_default
{
	{ "name",     "ircd.db.compression.default"              },
	{ "default",  "kZSTD;kLZ4Compression;kSnappyCompression" },
};

rocksdb::CompressionType
ircd::db::find_supported_compression(const std::string &input)
{
	rocksdb::CompressionType ret
	{
		rocksdb::kNoCompression
	};

	const auto &list
	{
		input == "default"?
			string_view{compression_default}:
			string_view{input}
	};

	tokens(list, ';', [&ret]
	(const string_view &requested)
	{
		if(ret != rocksdb::kNoCompression)
			return;

		for(const auto &[name, type] : db::compressions)
			if(type != 0L && name == requested)
			{
				ret = rocksdb::CompressionType(type);
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
		*read_only |= optstr_find_and_remove(optstr, "read_only=true;"s);
	else
		optstr_find_and_remove(optstr, "read_only=true;"s);

	// We also allow the user to specify fsck=true to run a repair operation on
	// the db. This may be expensive to do by default every startup.
	if(fsck)
		*fsck |= optstr_find_and_remove(optstr, "fsck=true;"s);
	else
		optstr_find_and_remove(optstr, "fsck=true;"s);

	// Generate RocksDB options from string
	rocksdb::DBOptions opts
	{
		db::options(optstr)
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

decltype(ircd::db::read_checksum)
ircd::db::read_checksum
{
	{ "name",     "ircd.db.read.checksum" },
	{ "default",  false                   }
};

namespace ircd::db
{
	static const rocksdb::ReadOptions default_read_options;
}

/// Convert our options structure into RocksDB's options structure.
[[gnu::hot]]
rocksdb::ReadOptions
ircd::db::make_opts(const gopts &opts)
{
	rocksdb::ReadOptions ret;
	assume(ret.iterate_lower_bound == nullptr);
	assume(ret.iterate_upper_bound == nullptr);
	assume(ret.iter_start_seqnum == 0);
	assume(ret.pin_data == false);
	assume(ret.fill_cache == true);
	assume(ret.total_order_seek == false);
	assume(ret.verify_checksums == true);
	assume(ret.tailing == false);
	assume(ret.read_tier == rocksdb::ReadTier::kReadAllTier);
	assume(ret.readahead_size == 0);
	assume(ret.prefix_same_as_start == false);

	ret.snapshot = opts.snapshot;
	ret.readahead_size = opts.readahead;

	// slice* for exclusive upper bound. when prefixes are used this value must
	// have the same prefix because ordering is not guaranteed between prefixes
	ret.iterate_lower_bound = opts.lower_bound;
	ret.iterate_upper_bound = opts.upper_bound;
	ret.iter_start_seqnum = opts.seqnum;

	ret.verify_checksums = bool(read_checksum);
	if(test(opts, get::CHECKSUM) & !test(opts, get::NO_CHECKSUM))
		ret.verify_checksums = true;

	if(test(opts, get::NO_SNAPSHOT))
		ret.tailing = true;

	if(test(opts, get::ORDERED))
		ret.total_order_seek = true;

	if(test(opts, get::PIN))
		ret.pin_data = true;

	if(test(opts, get::CACHE))
		ret.fill_cache = true;

	if(likely(test(opts, get::NO_CACHE)))
		ret.fill_cache = false;

	if(likely(test(opts, get::PREFIX)))
		ret.prefix_same_as_start = true;

	if(likely(test(opts, get::NO_BLOCKING)))
		ret.read_tier = rocksdb::ReadTier::kBlockCacheTier;

	return ret;
}

decltype(ircd::db::enable_wal)
ircd::db::enable_wal
{
	{ "name",      "ircd.db.wal.enable" },
	{ "default",   true                 },
	{ "persist",   false                },
};

[[gnu::hot]]
rocksdb::WriteOptions
ircd::db::make_opts(const sopts &opts)
{
	rocksdb::WriteOptions ret;
	//ret.no_slowdown = true;    // read_tier = NON_BLOCKING for writes

	ret.sync = test(opts, set::FSYNC);
	ret.disableWAL = !enable_wal || test(opts, set::NO_JOURNAL);
	ret.ignore_missing_column_families = test(opts, set::NO_COLUMN_ERR);
	ret.no_slowdown = test(opts, set::NO_BLOCKING);
	ret.low_pri = test(opts, set::PRIO_LOW);
	return ret;
}

//
//
//

std::vector<std::string>
ircd::db::available()
{
	const string_view &prefix
	{
		fs::base::db
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

		for(const auto &cpdir : checkpoints) try
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
	const auto &prefix
	{
		fs::base::db
	};

	const string_view parts[]
	{
		prefix, name, lex_cast(checkpoint)
	};

	return fs::path_string(parts);
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

//
// Iterator
//

[[gnu::hot]]
std::pair<ircd::string_view, ircd::string_view>
ircd::db::operator*(const rocksdb::Iterator &it)
{
	return { key(it), val(it) };
}

[[gnu::hot]]
ircd::string_view
ircd::db::key(const rocksdb::Iterator &it)
{
	return slice(it.key());
}

[[gnu::hot]]
ircd::string_view
ircd::db::val(const rocksdb::Iterator &it)
{
	return slice(it.value());
}

//
// PinnableSlice
//

[[gnu::hot]]
size_t
ircd::db::size(const rocksdb::PinnableSlice &ps)
{
	return size(static_cast<const rocksdb::Slice &>(ps));
}

[[gnu::hot]]
const char *
ircd::db::data(const rocksdb::PinnableSlice &ps)
{
	return data(static_cast<const rocksdb::Slice &>(ps));
}

[[gnu::hot]]
ircd::string_view
ircd::db::slice(const rocksdb::PinnableSlice &ps)
{
	return slice(static_cast<const rocksdb::Slice &>(ps));
}

//
// Slice
//

[[gnu::hot]]
size_t
ircd::db::size(const rocksdb::Slice &slice)
{
	return slice.size();
}

[[gnu::hot]]
const char *
ircd::db::data(const rocksdb::Slice &slice)
{
	return slice.data();
}

[[gnu::hot]]
rocksdb::Slice
ircd::db::slice(const string_view &sv)
{
	return { sv.data(), sv.size() };
}

[[gnu::hot]]
ircd::string_view
ircd::db::slice(const rocksdb::Slice &sk)
{
	return { sk.data(), sk.size() };
}

//
// reflect
//

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
ircd::db::reflect(const rocksdb::FlushReason &r)
{
	using FlushReason = rocksdb::FlushReason;

	switch(r)
	{
		case FlushReason::kOthers:                 return "Others";
		case FlushReason::kGetLiveFiles:           return "GetLiveFiles";
		case FlushReason::kShutDown:               return "ShutDown";
		case FlushReason::kExternalFileIngestion:  return "ExternalFileIngestion";
		case FlushReason::kManualCompaction:       return "ManualCompaction";
		case FlushReason::kWriteBufferManager:     return "WriteBufferManager";
		case FlushReason::kWriteBufferFull:        return "WriteBufferFull";
		case FlushReason::kTest:                   return "Test";
		case FlushReason::kDeleteFiles:            return "DeleteFiles";
		case FlushReason::kAutoCompaction:         return "AutoCompaction";
		case FlushReason::kManualFlush:            return "ManualFlush";
		case FlushReason::kErrorRecovery:          return "kErrorRecovery";
	}

	return "??????";
}

ircd::string_view
ircd::db::reflect(const rocksdb::CompactionReason &r)
{
	using CompactionReason = rocksdb::CompactionReason;

	switch(r)
	{
		case CompactionReason::kUnknown:                      return "Unknown";
		case CompactionReason::kLevelL0FilesNum:              return "LevelL0FilesNum";
		case CompactionReason::kLevelMaxLevelSize:            return "LevelMaxLevelSize";
		case CompactionReason::kUniversalSizeAmplification:   return "UniversalSizeAmplification";
		case CompactionReason::kUniversalSizeRatio:           return "UniversalSizeRatio";
		case CompactionReason::kUniversalSortedRunNum:        return "UniversalSortedRunNum";
		case CompactionReason::kFIFOMaxSize:                  return "FIFOMaxSize";
		case CompactionReason::kFIFOReduceNumFiles:           return "FIFOReduceNumFiles";
		case CompactionReason::kFIFOTtl:                      return "FIFOTtl";
		case CompactionReason::kManualCompaction:             return "ManualCompaction";
		case CompactionReason::kFilesMarkedForCompaction:     return "FilesMarkedForCompaction";
		case CompactionReason::kBottommostFiles:              return "BottommostFiles";
		case CompactionReason::kTtl:                          return "Ttl";
		case CompactionReason::kFlush:                        return "Flush";
		case CompactionReason::kExternalSstIngestion:         return "ExternalSstIngestion";

		#ifdef IRCD_DB_HAS_PERIODIC_COMPACTIONS
		case CompactionReason::kPeriodicCompaction:           return "kPeriodicCompaction";
		#endif

		case CompactionReason::kNumOfReasons:
			break;
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
		#if 0 // unreleased
		case BackgroundErrorReason::kManifestWrite:  return "MANIFESTWRITE";
		#endif
	}

	return "??????";
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
ircd::db::reflect(const rocksdb::Env::Priority &p)
{
	switch(p)
	{
		case rocksdb::Env::Priority::BOTTOM:  return "BOTTOM"_sv;
		case rocksdb::Env::Priority::LOW:     return "LOW"_sv;
		case rocksdb::Env::Priority::HIGH:    return "HIGH"_sv;
		#ifdef IRCD_DB_HAS_ENV_PRIO_USER
		case rocksdb::Env::Priority::USER:    return "USER"_sv;
		#endif
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
		case rocksdb::Env::IOPriority::IO_TOTAL:   break;
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
ircd::db::reflect(const rocksdb::Status::Code &s)
{
	using Code = rocksdb::Status::Code;

	switch(s)
	{
		case Code::kOk:                    return "Ok";
		case Code::kNotFound:              return "NotFound";
		case Code::kCorruption:            return "Corruption";
		case Code::kNotSupported:          return "NotSupported";
		case Code::kInvalidArgument:       return "InvalidArgument";
		case Code::kIOError:               return "IOError";
		case Code::kMergeInProgress:       return "MergeInProgress";
		case Code::kIncomplete:            return "Incomplete";
		case Code::kShutdownInProgress:    return "ShutdownInProgress";
		case Code::kTimedOut:              return "TimedOut";
		case Code::kAborted:               return "Aborted";
		case Code::kBusy:                  return "Busy";
		case Code::kExpired:               return "Expired";
		case Code::kTryAgain:              return "TryAgain";
		case Code::kCompactionTooLarge:    return "CompactionTooLarge";

		#if ROCKSDB_MAJOR > 6 \
		|| (ROCKSDB_MAJOR == 6 && ROCKSDB_MINOR > 3) \
		|| (ROCKSDB_MAJOR == 6 && ROCKSDB_MINOR == 3 && ROCKSDB_PATCH >= 6)
		case Code::kColumnFamilyDropped:   return "ColumnFamilyDropped";
		case Code::kMaxCode:               break;
		#endif
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
