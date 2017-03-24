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
	struct ColumnFamilyHandle;
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

template<class T> using optlist = std::initializer_list<optval<T>>;
template<class T> bool has_opt(const optlist<T> &, const T &);
template<class T> ssize_t opt_val(const optlist<T> &, const T &);

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
	READ_ONLY,              // Opens database without writing permitted.
	OPEN_FAST,              // Skips a lot of stuff to make opening a handle faster
	OPEN_SMALL,             // Optimizes the cache hierarchy for < 1GiB databases.
	OPEN_BULKLOAD,          // Optimizes the handle to accept a large amount of writes at once
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
	LRU_CACHE,              // Pair with a size in bytes for the LRU cache size
};

struct opts
:optlist<opt>
{
	template<class... list> opts(list&&... l): optlist<opt>{std::forward<list>(l)...} {}
};

enum op
{
	GET,
	SET,
	MERGE,
	DELETE,
	DELETE_RANGE,
	SINGLE_DELETE,
};

struct delta
:std::tuple<op, string_view, string_view>
{
	delta(const enum op &op, const string_view &key, const string_view &val = {})
	:std::tuple<enum op, string_view, string_view>{op, key, val}
	{}

	delta(const string_view &key, const string_view &val, const enum op &op = op::SET)
	:std::tuple<enum op, string_view, string_view>{op, key, val}
	{}
};

using merge_delta = std::pair<string_view, string_view>;
using merge_function = std::function<std::string (const string_view &key, const merge_delta &)>;
using update_function = std::function<std::string (const string_view &key, merge_delta &)>;

struct handle
{
	struct const_iterator;

  private:
	std::shared_ptr<struct meta> meta;
	rocksdb::ColumnFamilyHandle *h;

  public:
	using closure = std::function<void (const string_view &)>;

	operator bool() const                        { return bool(h);                                 }
	bool operator!() const                       { return !h;                                      }

	// Iterations
	const_iterator lower_bound(const string_view &key, const gopts & = {});
	const_iterator upper_bound(const string_view &key, const gopts & = {});
	const_iterator cbegin(const gopts & = {});
	const_iterator cend(const gopts & = {});

	// Perform a get into a closure. This offers a reference to the data with zero-copy.
	void operator()(const string_view &key, const closure &func, const gopts & = {});
	void operator()(const string_view &key, const gopts &, const closure &func);

	// Perform operations in a sequence as a single transaction.
	void operator()(const delta &, const sopts & = {});
	void operator()(const std::initializer_list<delta> &, const sopts & = {});
	void operator()(const op &, const string_view &key, const string_view &val = {}, const sopts & = {});

	// Tests if key exists
	bool has(const string_view &key, const gopts & = {});

	// Get data into your buffer. The signed char buffer is null terminated; the unsigned is not.
	size_t get(const string_view &key, char *const &buf, const size_t &max, const gopts & = {});
	size_t get(const string_view &key, uint8_t *const &buf, const size_t &max, const gopts & = {});
	std::string get(const string_view &key, const gopts & = {});

	// Write data to the db
	void set(const string_view &key, const string_view &value, const sopts & = {});
	void set(const string_view &key, const uint8_t *const &buf, const size_t &size, const sopts & = {});

	// Remove data from the db. not_found is never thrown.
	void del(const string_view &key, const sopts & = {});

	// Flush memory tables to disk (this column only).
	void flush(const bool &blocking = false);

	// Sync the write log (all columns)
	void sync();

	handle(const std::string &name,
	       const std::string &column  = "default",
	       const opts &               = {},
	       merge_function             = {});

	handle();
	handle(handle &&) noexcept;
	handle &operator=(handle &&) noexcept;
	~handle() noexcept;
};

struct handle::const_iterator
{
	using key_type = string_view;
	using mapped_type = string_view;
	using value_type = std::pair<key_type, mapped_type>;
	using pointer = value_type *;
	using reference = value_type &;
	using difference_type = size_t;
	using iterator_category = std::bidirectional_iterator_tag;

  private:
	struct state;
	friend class handle;

	std::unique_ptr<struct state> state;
	mutable value_type val;

	const_iterator(std::unique_ptr<struct state> &&);

  public:
	operator bool() const;
	bool operator!() const;
	bool operator<(const const_iterator &) const;
	bool operator>(const const_iterator &) const;
	bool operator==(const const_iterator &) const;
	bool operator!=(const const_iterator &) const;
	bool operator<=(const const_iterator &) const;
	bool operator>=(const const_iterator &) const;

	const value_type *operator->() const;
	const value_type &operator*() const;

	const_iterator &operator++();
	const_iterator &operator--();

	const_iterator() = default;
	const_iterator(const_iterator &&) noexcept;
	const_iterator(const const_iterator &) = delete;
	~const_iterator() noexcept;
};

handle::const_iterator begin(handle &);
handle::const_iterator end(handle &);

struct init
{
	init();
	~init() noexcept;
};

// db subsystem has its own SNOMASK'ed logging facility.
extern struct log::log log;

} // namespace db
} // namespace ircd

namespace ircd {
namespace db   {
namespace json {

std::string merge_operator(const string_view &, const std::pair<string_view, string_view> &);

struct obj
:handle
{
	obj(const std::string &name,
	    const std::string &column = "default",
	    const opts &opts = {})
	:handle{name, column, opts, merge_operator}
	{}
};

} // namespace json
} // namespace db
} // namespace ircd

inline ircd::db::handle::const_iterator
ircd::db::end(handle &handle)
{
	return handle.cend();
}

inline ircd::db::handle::const_iterator
ircd::db::begin(handle &handle)
{
	return handle.cbegin();
}

template<class T>
ssize_t
ircd::db::opt_val(const optlist<T> &list,
                  const T &opt)
{
	for(const auto &p : list)
		if(p.first == opt)
			return p.second;

	return std::numeric_limits<ssize_t>::min();
}

template<class T>
bool
ircd::db::has_opt(const optlist<T> &list,
                  const T &opt)
{
	for(const auto &p : list)
		if(p.first == opt)
			return true;

	return false;
}

template<class T>
ircd::db::optval<T>::optval(const T &key,
                            const ssize_t &val)
:std::pair<T, ssize_t>{key, val}
{
}
