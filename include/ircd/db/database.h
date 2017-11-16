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
#define HAVE_IRCD_DB_DATABASE_H

//
// Database instance
//
// There can be only one instance of this class for each database, so it is
// always shared and must be make_shared(). The database is open when an
// instance is constructed and closed when the instance destructs.
//
// The construction must have the same consistent descriptor set used every
// time otherwise bad things happen.
//
// The instance registers and deregisters itself in a global set of open
// databases and can be found that way if necessary.
//

namespace ircd::db
{
	struct database;

	template<class R = uint64_t> R property(database &, const string_view &name);
	template<> uint64_t property(database &, const string_view &name);
	const std::string &name(const database &);
	uint64_t sequence(const database &);             // Latest sequence number
	void sync(database &);                           // Sync the write log (all columns)
}

struct ircd::db::database
:std::enable_shared_from_this<database>
{
	struct descriptor;
	struct options;
	struct events;
	struct stats;
	struct logs;
	struct mergeop;
	struct snapshot;
	struct comparator;
	struct prefix_transform;
	struct column;

	using description = std::vector<descriptor>;

	// central collection of open databases for iteration (non-owning)
	static std::map<string_view, database *> dbs;

	std::string name;
	std::string path;
	std::string optstr;
	std::shared_ptr<struct logs> logs;
	std::shared_ptr<struct stats> stats;
	std::shared_ptr<struct events> events;
	std::shared_ptr<struct mergeop> mergeop;
	std::shared_ptr<rocksdb::Cache> cache;
	std::vector<descriptor> descriptors;
	std::vector<string_view> column_names;
	std::unordered_map<string_view, size_t> column_index;
	std::vector<std::shared_ptr<column>> columns;
	custom_ptr<rocksdb::DB> d;
	unique_const_iterator<decltype(dbs)> dbs_it;

	operator std::shared_ptr<database>()         { return shared_from_this();                      }
	operator const rocksdb::DB &() const         { return *d;                                      }
	operator rocksdb::DB &()                     { return *d;                                      }

	const column &operator[](const uint32_t &id) const;
	const column &operator[](const string_view &name) const;
	column &operator[](const uint32_t &id);
	column &operator[](const string_view &name);

	// [SET] Perform operations in a sequence as a single transaction.
	void operator()(const sopts &, const delta *const &begin, const delta *const &end);
	void operator()(const sopts &, const std::initializer_list<delta> &);
	void operator()(const sopts &, const delta &);
	void operator()(const delta *const &begin, const delta *const &end);
	void operator()(const std::initializer_list<delta> &);
	void operator()(const delta &);

    database(std::string name,
             std::string options,
             description);

    database(std::string name,
             std::string options = {});

	database() = default;
	database(database &&) = delete;
	database(const database &) = delete;
    ~database() noexcept;

	// Get this instance from any column.
	static const database &get(const column &);
	static database &get(column &);
};

namespace ircd::db
{
	std::shared_ptr<const database::column> shared_from(const database::column &);
	std::shared_ptr<database::column> shared_from(database::column &);
	const database::descriptor &describe(const database::column &);
	const std::string &name(const database::column &);
	uint32_t id(const database::column &);
	void drop(database::column &);                   // Request to erase column from db
}

// Descriptor of a column when opening database. Database must be opened with
// a consistent set of descriptors describing what will be found upon opening.
struct ircd::db::database::descriptor
{
	using typing = std::pair<std::type_index, std::type_index>;

	std::string name;
	std::string explain;
	typing type { typeid(string_view), typeid(string_view) };
	std::string options {};
	db::comparator cmp {};
	db::prefix_transform prefix {};
};

// options <-> string
struct ircd::db::database::options
:std::string
{
	struct map;

	// Output of options structures from this string
	explicit operator rocksdb::Options() const;
	operator rocksdb::DBOptions() const;
	operator rocksdb::ColumnFamilyOptions() const;
	operator rocksdb::PlainTableOptions() const;
	operator rocksdb::BlockBasedTableOptions() const;

	// Input of options structures output to this string
	explicit options(const rocksdb::ColumnFamilyOptions &);
	explicit options(const rocksdb::DBOptions &);
	explicit options(const database::column &);
	explicit options(const database &);

	// Input of options string from user
	options(std::string string)
	:std::string{std::move(string)}
	{}
};

// options <-> map
struct ircd::db::database::options::map
:std::unordered_map<std::string, std::string>
{
	// Output of options structures from map
	operator rocksdb::DBOptions() const;
	operator rocksdb::ColumnFamilyOptions() const;
	operator rocksdb::PlainTableOptions() const;
	operator rocksdb::BlockBasedTableOptions() const;

	// Convert option string to map
	map(const options &);

	// Input of options map from user
	map(std::unordered_map<std::string, std::string> m)
	:std::unordered_map<std::string, std::string>{std::move(m)}
	{}
};

struct ircd::db::database::snapshot
{
	std::shared_ptr<const rocksdb::Snapshot> s;

  public:
	operator const rocksdb::Snapshot *() const   { return s.get();                                 }

	explicit operator bool() const               { return bool(s);                                 }
	bool operator !() const                      { return !s;                                      }

	explicit snapshot(database &);
	snapshot() = default;
	~snapshot() noexcept;

	friend uint64_t sequence(const snapshot &);  // Sequence of a snapshot
	friend uint64_t sequence(const rocksdb::Snapshot *const &);
};
