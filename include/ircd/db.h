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

#pragma once
#define HAVE_IRCD_DB_H

namespace rocksdb
{
	struct DB;
}

namespace ircd {
namespace db   {

IRCD_EXCEPTION(ircd::error, error)
IRCD_EXCEPTION(error, not_found)
IRCD_EXCEPTION(error, corruption)
IRCD_EXCEPTION(error, not_supported)
IRCD_EXCEPTION(error, invalid_argument)
IRCD_EXCEPTION(error, io_error)
IRCD_EXCEPTION(error, merge_in_progress)
IRCD_EXCEPTION(error, incomplete)
IRCD_EXCEPTION(error, shutdown_in_progress)
IRCD_EXCEPTION(error, timed_out)
IRCD_EXCEPTION(error, aborted)
IRCD_EXCEPTION(error, busy)
IRCD_EXCEPTION(error, expired)
IRCD_EXCEPTION(error, try_again)

std::string path(const std::string &name);

template<class T>
struct optval
:std::pair<T, ssize_t>
{
	optval(const T &key, const ssize_t &val = std::numeric_limits<ssize_t>::min());
};

template<class T>
using optlist = std::initializer_list<optval<T>>;

// Reads may be posted to a separate thread which incurs the time of IO while the calling
// ircd::context yields.
enum class get
{
	PIN,                    // Keep iter data in memory for iter lifetime (good for lots of ++/--)
	CACHE,                  // Update the cache (CACHE is default for non-iterator operations)
	NO_CACHE,               // Do not update the cache (NO_CACHE is default for iterators)
	NO_SNAPSHOT,            // Snapshots provide consistent views for iteration.
	NO_CHECKSUM,            // Integrity of data will be checked unless this is specified
	READAHEAD,              // Pair with a size in bytes for prefetching additional data
};

struct gopts
:optlist<get>
{
	template<class... list> gopts(list&&... l): optlist<get>{std::forward<list>(l)...} {}
};

// Writes usually occur without yielding your context because the DB is write-log oriented.
enum class set
{
	FSYNC,                  // Uses kernel filesystem synchronization after write (slow)
	NO_JOURNAL,             // Write Ahead Log (WAL) for some crash recovery
	MISSING_COLUMNS         // No exception thrown when writing to a deleted column family
};

struct sopts
:optlist<set>
{
	template<class... list> sopts(list&&... l): optlist<set>{std::forward<list>(l)...} {}
};

enum class opt
{
	NO_CREATE,              // A new database may be created (if none found) unless this is specified
	NO_EXISTING,            // An error is given if database already exists
	NO_CHECKSUM,            // (paranoid_checks)
	NO_MADV_DONTNEED,       // Never issue MADV_DONTNEED (on windows turns off all pagecaching!)
	NO_MADV_RANDOM,         // Skip MADV_RANDOM on database file opening
	FALLOCATE,              // Allow use of fallocate()
	NO_FALLOCATE,           // Disallow use fallocate() calls
	NO_FDATASYNC,           // Flushing is only ever directed by the kernel pagecache
	FSYNC,                  // Use fsync() instead of fdatasync()
	MMAP_READS,             // mmap() table files for reading
	MMAP_WRITES,            // mmap() table files for writing (hinders db journal)
	STATS_THREAD,           // Stats collection etc related to DB threading (thread_track)
	STATS_MALLOC,           // Stats collection for memory allocation when applicable
	OPEN_FAST,              // Skips a lot of stuff to make opening a handle faster
	OPEN_SMALL,             // Optimizes the cache hierarchy for < 1GiB databases.
	OPEN_BULKLOAD,          // Optimizes the handle to accept a large amount of writes at once
};

struct opts
:optlist<opt>
{
	template<class... list> opts(list&&... l): optlist<opt>{std::forward<list>(l)...} {}
};

class handle
{
	std::unique_ptr<struct meta> meta;
	std::unique_ptr<rocksdb::DB> d;

  public:
	using char_closure = std::function<void (const char *, size_t)>;
	using string_closure = std::function<void (const std::string &)>;

	bool has(const std::string &key, const gopts & = {});
	void get(const std::string &key, const char_closure &, const gopts & = {});
	void set(const std::string &key, const std::string &value, const sopts & = {});

	handle(const std::string &name, const opts & = {});
	~handle() noexcept;
};

struct init
{
	init();
	~init() noexcept;
};

} // namespace db
} // namespace ircd

template<class T>
ircd::db::optval<T>::optval(const T &key,
                            const ssize_t &val)
:std::pair<T, ssize_t>{key, val}
{
}
