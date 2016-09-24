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
#include <ircd/db_meta.h>

namespace ircd {
namespace db   {

using rocksdb::DB;

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
:meta{std::make_unique<struct meta>(name, path(name), opts)}
,d{[this, &name]
{
	DB *ptr;
	throw_on_error(DB::Open(meta->opts, path(name), &ptr));
	std::unique_ptr<DB> ret{ptr};
	return ret;
}()}
{
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
                const write_opts &opts)
{
	using rocksdb::WriteOptions;
	using rocksdb::Slice;

	const Slice k(key.data(), key.size());
	const Slice v(value.data(), value.size());
	throw_on_error(d->Put(WriteOptions(), k, v));
}

void
db::handle::get(const std::string &key,
                const char_closure &func,
                const read_opts &opts)
{
	using rocksdb::ReadOptions;
	using rocksdb::Iterator;
	using rocksdb::Slice;

	ReadOptions ropts;
	const Slice sk(key.data(), key.size());
	query([this, &sk, &func, &ropts]
	{
		const std::unique_ptr<Iterator> it(d->NewIterator(ropts));

		it->Seek(sk);
		throw_on_error(it->status());

		const auto &v(it->value());
		func(v.data(), v.size());
	});
}

bool
db::handle::has(const std::string &key,
                const read_opts &opts)
{
	using rocksdb::ReadOptions;
	using rocksdb::Iterator;
	using rocksdb::Slice;

	bool ret;
	ReadOptions ropts;
	const Slice k(key.data(), key.size());
	query([this, &k, &ret, &ropts]
	{
		if(!d->KeyMayExist(ropts, k, nullptr, nullptr))
		{
			ret = false;
			return;
		}

		const std::unique_ptr<Iterator> it(d->NewIterator(ropts));

		it->Seek(k);
		switch(it->status().code())
		{
			using rocksdb::Status;

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
	auto closure([func(std::move(func)), &eptr, &context, &done]
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
