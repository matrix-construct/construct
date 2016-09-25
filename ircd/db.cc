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

namespace ircd {
namespace db   {

struct log::log log
{
	"db", 'D'            // Database subsystem takes SNOMASK +D
};

void throw_on_error(const rocksdb::Status &);

rocksdb::WriteOptions make_opts(const sopts &);
rocksdb::ReadOptions make_opts(const gopts &);
rocksdb::Options make_opts(const opts &);

struct meta
{
	std::string name;
	std::string path;
	rocksdb::Options opts;
	std::shared_ptr<rocksdb::Cache> cache;
};

namespace work
{
	using closure = std::function<void () noexcept>;

	std::mutex mutex;
	std::condition_variable cond;
	std::deque<closure> queue;
	bool interruption;
	std::thread *thread;

	closure pop();
	void worker() noexcept;
	void push(closure &&);

	void fini();
	void init();
}

void throw_on_error(const rocksdb::Status &);
rocksdb::Iterator &seek(rocksdb::Iterator &, const iter::op &);
template<class T> bool has_opt(const optlist<T> &, const T &);
rocksdb::WriteOptions make_opts(const sopts &);
rocksdb::ReadOptions make_opts(const gopts &, const bool &iterator = false);
rocksdb::Options make_opts(const opts &);

void query(std::function<void ()>);

} // namespace db
} // namespace ircd

using namespace ircd;

db::init::init()
{
	work::init();
}

db::init::~init()
noexcept
{
	work::fini();
}

db::handle::handle(const std::string &name,
                   const opts &opts)
try
:meta{[&name, &opts]
{
	auto meta(std::make_unique<struct meta>());
	meta->name = name;
	meta->path = path(name);
	meta->opts = make_opts(opts);
	meta->opts.row_cache = meta->cache;
	return std::move(meta);
}()}
,d{[this]
{
	rocksdb::DB *ptr;
	throw_on_error(rocksdb::DB::Open(meta->opts, meta->path, &ptr));
	return std::unique_ptr<rocksdb::DB>{ptr};
}()}
{
	log.info("Opened database \"%s\" @ `%s' (handle: %p)",
	         meta->name.c_str(),
	         meta->path.c_str(),
	         (const void *)this);
}
catch(const invalid_argument &e)
{
	const bool no_create(has_opt(opts, opt::NO_CREATE));
	const bool no_existing(has_opt(opts, opt::NO_EXISTING));
	const char *const helpstr
	{
		no_create?   " (The database is missing and will not be created)":
		no_existing? " (The database already exists but must be fresh)":
		             ""
	};

	throw error("Failed to open db '%s': %s%s",
	            name.c_str(),
	            e.what(),
	            helpstr);
}
catch(const std::exception &e)
{
	throw error("Failed to open db '%s': %s",
	            name.c_str(),
	            e.what());
}

db::handle::~handle()
noexcept
{
}

void
db::handle::set(const std::string &key,
                const std::string &value,
                const sopts &sopts)
{
	using rocksdb::Slice;

	const Slice k(key.data(), key.size());
	const Slice v(value.data(), value.size());

	auto opts(make_opts(sopts));
	throw_on_error(d->Put(opts, k, v));
}

void
db::handle::get(const std::string &key,
                const char_closure &func,
                const gopts &gopts)
{
	using rocksdb::Iterator;
	using rocksdb::Slice;

	auto opts(make_opts(gopts));
	const Slice sk(key.data(), key.size());
	query([this, &sk, &func, &opts]
	{
		const std::unique_ptr<Iterator> it(d->NewIterator(opts));

		it->Seek(sk);
		throw_on_error(it->status());

		const auto &v(it->value());
		func(v.data(), v.size());
	});
}

bool
db::handle::has(const std::string &key,
                const gopts &gopts)
{
	using rocksdb::Slice;
	using rocksdb::Iterator;
	using rocksdb::Status;

	bool ret;
	auto opts(make_opts(gopts));
	const Slice k(key.data(), key.size());
	query([this, &k, &ret, &opts]
	{
		if(!d->KeyMayExist(opts, k, nullptr, nullptr))
		{
			ret = false;
			return;
		}

		const std::unique_ptr<Iterator> it(d->NewIterator(opts));

		it->Seek(k);
		switch(it->status().code())
		{
			case Status::kOk:         ret = true;   return;
			case Status::kNotFound:   ret = false;  return;
			default:
				throw_on_error(it->status());
		}
	});

	return ret;
}

void
db::query(std::function<void ()> func)
{
	std::exception_ptr eptr;
	auto &context(ctx::cur());
	std::atomic<bool> done{false};
	auto closure([&func, &eptr, &context, &done]
	() noexcept
	{
		try
		{
			func();
		}
		catch(...)
		{
			eptr = std::current_exception();
		}

		done.store(true, std::memory_order_release);
		notify(context);
	});

	work::push(std::move(closure)); do
	{
		ctx::wait();
	}
	while(!done.load(std::memory_order_consume));

	if(eptr)
		std::rethrow_exception(eptr);
}

void
db::work::init()
{
	assert(!thread);
	interruption = false;
	thread = new std::thread(&worker);
}

void
db::work::fini()
{
	if(!thread)
		return;

	mutex.lock();
	interruption = true;
	cond.notify_one();
	mutex.unlock();
	thread->join();
	delete thread;
	thread = nullptr;
}

void
db::work::push(closure &&func)
{
	const std::lock_guard<decltype(mutex)> lock(mutex);
	queue.emplace_back(std::move(func));
	cond.notify_one();
}

void
db::work::worker()
noexcept try
{
	while(1)
	{
		const auto func(pop());
		func();
	}
}
catch(const ctx::interrupted &)
{
	return;
}

db::work::closure
db::work::pop()
{
	std::unique_lock<decltype(mutex)> lock(mutex);
	cond.wait(lock, []
	{
		if(!queue.empty())
			return true;

		if(unlikely(interruption))
			throw ctx::interrupted();

		return false;
	});

	auto c(std::move(queue.front()));
	queue.pop_front();
	return std::move(c);
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

template<class T>
bool
db::has_opt(const optlist<T> &list,
            const T &opt)
{
	const auto check([&opt]
	(const auto &pair)
	{
		return pair.first == opt;
	});

	return std::find_if(begin(list), end(list), check) != end(list);
}

}

std::string
db::path(const std::string &name)
{
	const auto prefix(path::get(path::DB));
	return path::build({prefix, name});
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
