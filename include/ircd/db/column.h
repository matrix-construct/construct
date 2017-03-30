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
#define HAVE_IRCD_DB_COLUMN_H

//
// [GET] may be posted to a separate thread which incurs the time of IO while the calling
// ircd::context yields.
//
// [SET] usually occur without yielding your context because the DB is write-log oriented.
//

namespace ircd {
namespace db   {

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

enum class get
{
	PIN,                    // Keep iter data in memory for iter lifetime (good for lots of ++/--)
	CACHE,                  // Update the cache (CACHE is default for non-iterator operations)
	NO_CACHE,               // Do not update the cache (NO_CACHE is default for iterators)
	NO_SNAPSHOT,            // This iterator will have the latest data (tailing)
	NO_CHECKSUM,            // Integrity of data will be checked unless this is specified
	READAHEAD,              // Pair with a size in bytes for prefetching additional data
};

struct gopts
:optlist<get>
{
	database::snapshot snapshot;

	template<class... list> gopts(list&&... l): optlist<get>{std::forward<list>(l)...} {}
};

// Columns add the ability to run multiple LevelDB's in synchrony under the same database
// (directory). Each column is a fully distinct key/value store; they are merely joined
// for consistency.
//
struct column
{
	struct const_iterator;
	using key_type = string_view;
	using mapped_type = string_view;
	using value_type = std::pair<key_type, mapped_type>;
	using pointer = value_type *;
	using reference = value_type &;
	using difference_type = size_t;
	using iterator = const_iterator;

  protected:
	using ColumnFamilyHandle = rocksdb::ColumnFamilyHandle;

	std::shared_ptr<database> d;
	database::column *c;

  public:
	operator const database &() const            { return *d;                                      }
	operator const database::column &() const    { return *c;                                      }
	operator std::shared_ptr<database>() const   { return d;                                       }

	operator database &()                        { return *d;                                      }
	operator database::column &()                { return *c;                                      }

	operator bool() const                        { return bool(d);                                 }
	bool operator!() const                       { return !d;                                      }

	// [GET] Iterations
	const_iterator cbegin(const gopts & = {});
	const_iterator cend(const gopts & = {});
	const_iterator begin(const gopts & = {});
	const_iterator end(const gopts & = {});
	const_iterator find(const string_view &key, const gopts & = {});
	const_iterator lower_bound(const string_view &key, const gopts & = {});
	const_iterator upper_bound(const string_view &key, const gopts & = {});

	// [GET] Tests if key exists
	bool has(const string_view &key, const gopts & = {});

	// [GET] Perform a get into a closure. This offers a reference to the data with zero-copy.
	void operator()(const string_view &key, const view_closure &func, const gopts & = {});
	void operator()(const string_view &key, const gopts &, const view_closure &func);

	// [SET] Perform operations in a sequence as a single transaction.
	void operator()(const delta &, const sopts & = {});
	void operator()(const std::initializer_list<delta> &, const sopts & = {});
	void operator()(const op &, const string_view &key, const string_view &val = {}, const sopts & = {});

	// Flush memory tables to disk (this column only).
	void flush(const bool &blocking = false);

	column(std::shared_ptr<database>, database::column &);
	column(database &, database::column &);
	column(database &, const string_view &column);
	column(database::column &);
	column() = default;
};

// Get property data of a db column (column).
// R can optionally be uint64_t for some values.
template<class R = std::string> R property(column &, const string_view &name);
template<> std::string property(column &, const string_view &name);
template<> uint64_t property(column &, const string_view &name);

// Information about a column (column)
const std::string &name(const column &);
size_t file_count(column &);
size_t bytes(column &);

// [GET] Convenience functions to copy data into your buffer.
// The signed char buffer is null terminated; the unsigned is not.
size_t read(column &, const string_view &key, uint8_t *const &buf, const size_t &max, const gopts & = {});
string_view read(column &, const string_view &key, char *const &buf, const size_t &max, const gopts & = {});
std::string read(column &, const string_view &key, const gopts & = {});

// [SET] Write data to the db
void write(column &, const string_view &key, const string_view &value, const sopts & = {});
void write(column &, const string_view &key, const uint8_t *const &buf, const size_t &size, const sopts & = {});

// [SET] Remove data from the db. not_found is never thrown.
void del(column &, const string_view &key, const sopts & = {});

} // namespace db
} // namespace ircd
