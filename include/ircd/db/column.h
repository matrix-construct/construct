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

// Columns add the ability to run multiple LevelDB's in synchrony under the same
// database (directory). Each column is a fully distinct key/value store; they
// are merely joined for consistency and possible performance advantages for
// concurrent multi-column lookups of the same key.
//
// This class is a handle to the real column instance `database::column` because the
// real column instance has to have a lifetime congruent to the open database. But
// that makes this object easier to work with, pass around, and construct. It will
// find the real `database::column` at any time.
//
// [GET] If the data is not cached, your ircd::context will yield. Note that the
// request may be posted to a separate thread which incurs the time of IO. This is
// because RocksDB has minimalist origins and is not yet asynchronous.
//
// + In the future, your ircd::context will still yield but the internals here will
// interleave pending contexts. If RocksDB is clever enough to expose actual file
// descriptors or something we can interleave on, similar to the pgsql API, we can
// remove the IO/offload thread as well.
//
// [SET] usually occur without yielding your context because the DB is oriented
// around write-log appends. It deals with the heavier tasks later in background.
//
// NOTE that the column and cell structs are type-agnostic. The database is capable of
// storing binary data in the key or the value for a cell. The string_view will work
// with both a normal string and binary data, so this class is not a template and
// offers no conversions at this level. see: value.h/object.h
//
namespace ircd::db
{
	struct column;

	// Get property data of a db column. R can optionally be uint64_t for some
	// values. Refer to RocksDB documentation for more info.
	template<class R = std::string> R property(column &, const string_view &name);
	template<> std::string property(column &, const string_view &name);
	template<> uint64_t property(column &, const string_view &name);

	// Information about a column
	const std::string &name(const column &);
	size_t file_count(column &);
	size_t bytes(column &);

	// [GET] Tests if key exists
	bool has(column &, const string_view &key, const gopts & = {});

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

	// [SET] Flush memory tables to disk (this column only).
	void flush(column &, const bool &blocking = false);
}

struct ircd::db::column
{
	struct delta;
	struct const_iterator;
	using key_type = string_view;
	using mapped_type = string_view;
	using value_type = std::pair<key_type, mapped_type>;
	using pointer = value_type *;
	using reference = value_type &;
	using difference_type = size_t;
	using iterator = const_iterator;

  protected:
	database::column *c;

  public:
	explicit operator const database &() const;
	explicit operator const database::column &() const;

	explicit operator database &();
	explicit operator database::column &();

	operator bool() const                        { return bool(c);                                 }
	bool operator!() const                       { return !c;                                      }

	// [GET] Iterations
	const_iterator cbegin(const gopts & = {});
	const_iterator cend(const gopts & = {});
	const_iterator begin(const gopts & = {});
	const_iterator end(const gopts & = {});
	const_iterator find(const string_view &key, const gopts & = {});
	const_iterator lower_bound(const string_view &key, const gopts & = {});
	const_iterator upper_bound(const string_view &key, const gopts & = {});

	// [GET] Get cell
	cell operator[](const string_view &key) const;

	// [GET] Perform a get into a closure. This offers a reference to the data with zero-copy.
	using view_closure = std::function<void (const string_view &)>;
	void operator()(const string_view &key, const view_closure &func, const gopts & = {});
	void operator()(const string_view &key, const gopts &, const view_closure &func);

	// [SET] Perform operations in a sequence as a single transaction. No template iterators
	// supported yet, just a ContiguousContainer iteration (and derived convenience overloads)
	void operator()(const delta *const &begin, const delta *const &end, const sopts & = {});
	void operator()(const std::initializer_list<delta> &, const sopts & = {});
	void operator()(const sopts &, const std::initializer_list<delta> &);
	void operator()(const delta &, const sopts & = {});

	column(database::column &c);
	column(database &, const string_view &column);
	column() = default;
};

//
// Delta is an element of a transaction. Use column::delta's to atomically
// commit to multiple keys in the same column. Refer to delta.h for the `enum op`
// choices. Refer to cell::delta to transact with multiple cells across different
// columns. Refer to row::delta to transact with entire rows.
//
// Note, for now, unlike cell::delta and row::delta, the column::delta has
// no reference to the column in its tuple. This is why these deltas are executed
// through the member column::operator() and not an overload of db::write().
//
// It is unlikely you will need to work with column deltas directly because
// you may decohere one column from the others participating in a row.
//
struct ircd::db::column::delta
:std::tuple<op, string_view, string_view>
{
	delta(const string_view &key, const string_view &val, const enum op &op = op::SET)
	:std::tuple<enum op, string_view, string_view>{op, key, val}
	{}

	delta(const enum op &op, const string_view &key, const string_view &val = {})
	:std::tuple<enum op, string_view, string_view>{op, key, val}
	{}
};

//
// Iteration over all keys down a column. Default construction is an invalid
// iterator, which could be compared against in the style of STL algorithms.
// Otherwise, construct an iterator by having it returned from the appropriate
// function in column::.
//
struct ircd::db::column::const_iterator
{
	using value_type = column::value_type;
	using iterator_category = std::bidirectional_iterator_tag;

  private:
	gopts opts;
	database::column *c;
	std::unique_ptr<rocksdb::Iterator> it;
	mutable value_type val;

	friend class column;
	const_iterator(database::column *const &, std::unique_ptr<rocksdb::Iterator> &&, gopts = {});

  public:
	explicit operator const database::snapshot &() const;
	explicit operator const database::column &() const;
	explicit operator const gopts &() const;

	explicit operator database::snapshot &();
	explicit operator database::column &();

	operator bool() const;
	bool operator!() const;

	const value_type *operator->() const;
	const value_type &operator*() const;

	const_iterator &operator++();
	const_iterator &operator--();

	const_iterator();
	const_iterator(const_iterator &&) noexcept;
	const_iterator &operator=(const_iterator &&) noexcept;
	~const_iterator() noexcept;

	friend bool operator==(const const_iterator &, const const_iterator &);
	friend bool operator!=(const const_iterator &, const const_iterator &);
	friend bool operator<(const const_iterator &, const const_iterator &);
	friend bool operator>(const const_iterator &, const const_iterator &);

	template<class pos> friend bool seek(column::const_iterator &, const pos &);
};

inline ircd::db::column::const_iterator::operator
database::column &()
{
	return *c;
}

inline ircd::db::column::const_iterator::operator
database::snapshot &()
{
	return opts.snapshot;
}

inline ircd::db::column::const_iterator::operator
const gopts &()
const
{
	return opts;
}

inline ircd::db::column::const_iterator::operator
const database::column &()
const
{
	return *c;
}

inline ircd::db::column::const_iterator::operator
const database::snapshot &()
const
{
	return opts.snapshot;
}

inline ircd::db::column::operator
database::column &()
{
	return *c;
}

inline ircd::db::column::operator
database &()
{
	return database::get(*c);
}

inline ircd::db::column::operator
const database::column &()
const
{
	return *c;
}

inline ircd::db::column::operator
const database &()
const
{
	return database::get(*c);
}
