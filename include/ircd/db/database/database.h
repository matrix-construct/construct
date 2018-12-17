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

	// Broad conf items
	extern conf::item<bool> open_check;
	extern conf::item<std::string> open_recover;

	// General information
	const std::string &name(const database &);
	const std::string &uuid(const database &);
	uint64_t sequence(const database &); // Latest sequence number
	const std::vector<std::string> &errors(const database &);
	std::vector<std::string> files(const database &, uint64_t &msz);
	std::vector<std::string> files(const database &);
	std::vector<std::string> wals(const database &);
	size_t file_count(const database &);
	size_t bytes(const database &);
	options getopt(const database &);
	log::facility loglevel(const database &);

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
	void loglevel(database &, const log::facility &);
	void setopt(database &, const string_view &key, const string_view &val);
	void fdeletions(database &, const bool &enable, const bool &force = false);
	uint64_t checkpoint(database &);
	void bgcancel(database &, const bool &blocking = true);
	void bgcontinue(database &);
	void bgpause(database &);
	void resume(database &);
	void check(database &);
	void compact(database &, const std::pair<int, int> &level, const compactor & = {});
	void compact(database &, const compactor & = {});
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
/// Internal structures declared within this class comprise the backend which
/// supports RocksDB; they are not involved in the standard include stack
/// beyond this declaration and not meant for IRCd developers merely using the
/// ircd::db interface.
///
struct ircd::db::database
:std::enable_shared_from_this<database>
,instance_list<database>
{
	struct options;
	struct events;
	struct stats;
	struct logger;
	struct mergeop;
	struct snapshot;
	struct comparator;
	struct prefix_transform;
	struct compaction_filter;
	struct column;
	struct env;
	struct cache;
	struct sst;
	struct wal;

	std::string name;
	uint64_t checkpoint;
	std::string path;
	std::string optstr;
	bool fsck, read_only;
	std::shared_ptr<struct env> env;
	std::shared_ptr<struct stats> stats;
	std::shared_ptr<struct logger> logger;
	std::shared_ptr<struct events> events;
	std::shared_ptr<struct mergeop> mergeop;
	std::shared_ptr<rocksdb::SstFileManager> ssts;
	std::shared_ptr<rocksdb::Cache> row_cache;
	std::vector<descriptor> descriptors;
	std::unique_ptr<rocksdb::DBOptions> opts;
	std::unordered_map<string_view, std::shared_ptr<column>> column_names;
	std::unique_ptr<rocksdb::DB> d;
	std::vector<std::shared_ptr<column>> column_index; // indexed by cfid
	std::list<std::shared_ptr<column>> columns; // active only
	std::string uuid;
	std::unique_ptr<rocksdb::Checkpoint> checkpointer;
	std::vector<std::string> errors;

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

	database(const string_view &name,
	         const uint64_t &checkpoint,
	         std::string options,
	         description);

	database(const string_view &name,
	         std::string options,
	         description);

	database(const string_view &name,
	         std::string options = {});

	database() = default;
	database(database &&) = delete;
	database(const database &) = delete;
	~database() noexcept;

	// Find this instance by name (and checkpoint id) in the instance list
	static database *get(std::nothrow_t, const string_view &name, const uint64_t &checkpoint);
	static database *get(std::nothrow_t, const string_view &name); // optionally "name:checkpoint"
	static database &get(const string_view &name, const uint64_t &checkpoint);
	static database &get(const string_view &name); // optionally "name:checkpoint"

	// Get this instance from any column.
	static const database &get(const column &);
	static database &get(column &);
};
