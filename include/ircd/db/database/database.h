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
#define HAVE_IRCD_DB_DATABASE_H

namespace ircd::db
{
	struct database;

	// General information
	const std::string &name(const database &);
	const std::string &uuid(const database &);
	uint64_t sequence(const database &); // Latest sequence number
	std::vector<std::string> files(const database &, uint64_t &msz);
	std::vector<std::string> files(const database &);
	size_t file_count(const database &);
	size_t bytes(const database &);

	// Property information interface
	using prop_int = uint64_t;
	using prop_str = std::string;
	using prop_map = std::map<std::string, std::string>;
	template<class R = prop_int> R property(const database &, const string_view &name);
	template<> prop_int property(const database &, const string_view &name);

	// Access to the database's row cache (see cache.h interface)
	const rocksdb::Cache *cache(const database &);
	rocksdb::Cache *cache(database &);

	// Control panel
	void setopt(database &, const string_view &key, const string_view &val);
	void fdeletions(database &, const bool &enable, const bool &force = false);
	void checkpoint(database &, const string_view &dir);
	void check(database &);
	void compact(database &);
	void sort(database &, const bool &blocking = true);
	void flush(database &, const bool &sync = false);
	void sync(database &);
}

/// Database instance
///
/// There can be only one instance of this class for each database, so it is
/// always shared and must be make_shared(). The database is open when an
/// instance is constructed and closed when the instance destructs.
///
/// The construction must have the same consistent descriptor set used every
/// time otherwise bad things happen.
///
/// The instance registers and deregisters itself in a global set of open
/// databases and can be found that way if necessary.
///
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
	struct env;

	using description = std::vector<descriptor>;

	// central collection of open databases for iteration (non-owning)
	static std::map<string_view, database *> dbs;

	std::string name;
	std::string path;
	std::string optstr;
	std::shared_ptr<struct env> env;
	std::shared_ptr<struct logs> logs;
	std::shared_ptr<struct stats> stats;
	std::shared_ptr<struct events> events;
	std::shared_ptr<struct mergeop> mergeop;
	std::shared_ptr<rocksdb::SstFileManager> ssts;
	std::shared_ptr<rocksdb::Cache> cache;
	std::vector<descriptor> descriptors;
	std::vector<string_view> column_names;
	std::unordered_map<string_view, size_t> column_index;
	std::vector<std::shared_ptr<column>> columns;
	std::unique_ptr<rocksdb::DB> d;
	std::string uuid;
	std::unique_ptr<rocksdb::Checkpoint> checkpoint;
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
