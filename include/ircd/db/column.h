// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_DB_COLUMN_H

namespace ircd::db
{
	struct column;

	// Information about a column
	uint32_t id(const column &);
	const std::string &name(const column &);
	const descriptor &describe(const column &);
	std::vector<std::string> files(const column &);
	size_t file_count(const column &);
	size_t bytes(const column &);
	options getopt(const column &);

	// Get property data of a db column. R can optionally be uint64_t for some
	// values; we typedef that as prop_int for templating purposes. R can also
	// be an std::string which we typedef as prop_str. Refer to RocksDB
	// documentation for more info.
	template<class R = prop_str> R property(const column &, const string_view &name);
	template<> prop_str property(const column &, const string_view &name);
	template<> prop_int property(const column &, const string_view &name);
	template<> prop_map property(const column &, const string_view &name);

	// Access to the column's caches (see cache.h interface)
	const rocksdb::Cache *cache_compressed(const column &);
	const rocksdb::Cache *cache(const column &);
	rocksdb::Cache *cache_compressed(column &);
	rocksdb::Cache *cache(column &);

	// [GET] Tests if key exists
	bool has(column &, const string_view &key, const gopts & = {});
	bool cached(column &, const string_view &key, const gopts & = {});
	void prefetch(column &, const string_view &key, const gopts & = {});

	// [GET] Convenience functions to copy data into your buffer.
	string_view read(column &, const string_view &key, const mutable_buffer &, const gopts & = {});
	std::string read(column &, const string_view &key, const gopts & = {});

	// [GET] Nothrow convenience functions to copy data into your buffer; since
	// a key can exist with an empty value we must overload on this bool here.
	string_view read(column &, const string_view &key, bool &found, const mutable_buffer &, const gopts & = {});
	std::string read(column &, const string_view &key, bool &found, const gopts & = {});

	// [SET] Write data to the db
	void write(column &, const string_view &key, const const_buffer &value, const sopts & = {});

	// [SET] Remove data from the db. not_found is never thrown.
	void del(column &, const string_view &key, const sopts & = {});

	// [SET] Other operations
	void ingest(column &, const string_view &path);
	void setopt(column &, const string_view &key, const string_view &val);
	void compact(column &, const std::pair<string_view, string_view> &, const int &to_level = -1, const compactor & = {});
	void compact(column &, const std::pair<int, int> &level = {-1, -1}, const compactor & = {});
	void sort(column &, const bool &blocking = false);
	void drop(column &); // danger
}

/// Columns add the ability to run multiple LevelDB's in synchrony under the same
/// database (directory). Each column is a fully distinct key/value store; they
/// are merely joined for consistency and possible performance advantages for
/// concurrent multi-column lookups of the same key.
///
/// This class is a handle to the real column instance `database::column` because the
/// real column instance has to have a lifetime congruent to the open database. But
/// that makes this object easier to work with, pass around, and construct. It will
/// find the real `database::column` at any time.
///
/// [GET] If the data is not cached, your ircd::context will yield.
///
/// [SET] usually occur without yielding your context because the DB is oriented
/// around write-log appends. It deals with the heavier tasks later in background.
///
/// NOTE that the column and cell structs are type-agnostic. The database is capable of
/// storing binary data in the key or the value for a cell. The string_view will work
/// with both a normal string and binary data, so this class is not a template and
/// offers no conversions at this interface.
///
struct ircd::db::column
{
	struct delta;
	struct const_iterator_base;
	struct const_iterator;
	struct const_reverse_iterator;

	using key_type = string_view;
	using mapped_type = string_view;
	using value_type = std::pair<key_type, mapped_type>;
	using pointer = value_type *;
	using reference = value_type &;
	using iterator = const_iterator;
	using reverse_iterator = const_reverse_iterator;
	using iterator_category = std::bidirectional_iterator_tag;
	using difference_type = size_t;

  protected:
	database::column *c {nullptr};

  public:
	explicit operator const database &() const;
	explicit operator const database::column &() const;
	explicit operator const descriptor &() const;

	explicit operator database &();
	explicit operator database::column &();

	explicit operator bool() const               { return bool(c);                                 }
	bool operator!() const                       { return !c;                                      }

	// [GET] Iterations
	const_iterator begin(gopts = {});
	const_iterator end(gopts = {});

	const_reverse_iterator rbegin(gopts = {});
	const_reverse_iterator rend(gopts = {});

	const_iterator find(const string_view &key, gopts = {});
	const_iterator lower_bound(const string_view &key, gopts = {});
	const_iterator upper_bound(const string_view &key, gopts = {});

	// [GET] Get cell
	cell operator[](const string_view &key) const;

	// [GET] Perform a get into a closure. This offers a reference to the data with zero-copy.
	using view_closure = std::function<void (const string_view &)>;
	bool operator()(const string_view &key, std::nothrow_t, const view_closure &func, const gopts & = {});
	bool operator()(const string_view &key, std::nothrow_t, const gopts &, const view_closure &func);
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

/// Delta is an element of a transaction. Use column::delta's to atomically
/// commit to multiple keys in the same column. Refer to delta.h for the `enum op`
/// choices. Refer to cell::delta to transact with multiple cells across different
/// columns. Refer to row::delta to transact with entire rows.
///
/// Note, for now, unlike cell::delta and row::delta, the column::delta has
/// no reference to the column in its tuple. This is why these deltas are executed
/// through the member column::operator() and not an overload of db::write().
///
/// It is unlikely you will need to work with column deltas directly because
/// you may decohere one column from the others participating in a row.
///
struct ircd::db::column::delta
:std::tuple<op, string_view, string_view>
{
	enum
	{
		OP, KEY, VAL,
	};

	delta(const string_view &key, const string_view &val, const enum op &op = op::SET)
	:std::tuple<enum op, string_view, string_view>{op, key, val}
	{}

	delta(const enum op &op, const string_view &key, const string_view &val = {})
	:std::tuple<enum op, string_view, string_view>{op, key, val}
	{}
};

/// Iteration over all keys down a column. Default construction is an invalid
/// iterator, which could be compared against in the style of STL algorithms.
/// Otherwise, construct an iterator by having it returned from the appropriate
/// function in column::.
///
struct ircd::db::column::const_iterator_base
{
	using key_type = string_view;
	using mapped_type = string_view;
	using value_type = std::pair<key_type, mapped_type>;
	using pointer = value_type *;
	using reference = value_type &;
	using iterator_category = std::bidirectional_iterator_tag;
	using difference_type = size_t;

  protected:
	database::column *c;
	gopts opts;
	std::unique_ptr<rocksdb::Iterator> it;
	mutable value_type val;

	const_iterator_base(database::column *const &, std::unique_ptr<rocksdb::Iterator> &&, db::gopts = {});

  public:
	explicit operator const database::column &() const;
	explicit operator const database::snapshot &() const;
	explicit operator const gopts &() const;

	explicit operator database::column &();
	explicit operator database::snapshot &();
	explicit operator gopts &();

	operator bool() const;
	bool operator!() const;

	const value_type *operator->() const;
	const value_type &operator*() const;

	const_iterator_base();
	const_iterator_base(const_iterator_base &&) noexcept;
	const_iterator_base &operator=(const_iterator_base &&) noexcept;
	~const_iterator_base() noexcept;

	friend bool operator==(const const_iterator_base &, const const_iterator_base &);
	friend bool operator!=(const const_iterator_base &, const const_iterator_base &);
	friend bool operator<(const const_iterator_base &, const const_iterator_base &);
	friend bool operator>(const const_iterator_base &, const const_iterator_base &);

	template<class pos> friend bool seek(column::const_iterator_base &, const pos &);
};

struct ircd::db::column::const_iterator
:const_iterator_base
{
	friend class column;

	const_iterator &operator++();
	const_iterator &operator--();

	using const_iterator_base::const_iterator_base;
};

struct ircd::db::column::const_reverse_iterator
:const_iterator_base
{
	friend class column;

	const_reverse_iterator &operator++();
	const_reverse_iterator &operator--();

	using const_iterator_base::const_iterator_base;
};

inline ircd::db::column::const_iterator_base::operator
gopts &()
{
	return opts;
}

inline ircd::db::column::const_iterator_base::operator
database::snapshot &()
{
	return opts.snapshot;
}

inline ircd::db::column::const_iterator_base::operator
database::column &()
{
	return *c;
}

inline ircd::db::column::const_iterator_base::operator
const gopts &()
const
{
	return opts;
}

inline ircd::db::column::const_iterator_base::operator
const database::snapshot &()
const
{
	return opts.snapshot;
}

inline ircd::db::column::const_iterator_base::operator
const database::column &()
const
{
	return *c;
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
