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

/// Uncomment or -D this #define to enable extensive log messages covering the
/// entire RocksDB callback surface. This is only useful for developers
/// specifically working on the backend of the DB and no real use for
/// developers making frontend queries to it. Massively verbose.
///
//#define RB_DEBUG_DB_ENV

/// This #define is more useful to developers making queries to the database.
/// It is still so verbose that it goes beyond what is tolerable and generally
/// useful even in debug-mode builds, thus the manual #define being required.
///
//#define RB_DEBUG_DB_SEEK

/// Uncomment or -D this #define to enable extensive log messages for the
/// experimental db environment-port implementation. This is only useful
/// for developers working on the port impl and want to debug all locking
/// and unlocking etc.
///
//#define RB_DEBUG_DB_PORT

/// Defined to enable our rocksdb::port implementation which connects to our
/// ircd::ctx threading implementation. This is experimental. Note: at this
/// time this MUST be enabled or rocksdb's will be using posix threading and
/// that will not work with our env.
#define IRCD_DB_PORT

namespace ircd::db
{
	struct throw_on_error;
	struct error_to_status;

	const auto BLOCKING = rocksdb::ReadTier::kReadAllTier;
	const auto NON_BLOCKING = rocksdb::ReadTier::kBlockCacheTier;

	// state
	extern log::log rog;
	extern conf::item<size_t> request_pool_size;
	extern conf::item<size_t> request_pool_stack_size;
	extern ctx::pool request;
	extern ctx::mutex write_mutex;

	// reflections
	string_view reflect(const rocksdb::Status::Severity &);
	string_view reflect(const rocksdb::Env::Priority &p);
	string_view reflect(const rocksdb::Env::IOPriority &p);
	string_view reflect(const rocksdb::Env::WriteLifeTimeHint &);
	string_view reflect(const rocksdb::WriteStallCondition &);
	string_view reflect(const rocksdb::BackgroundErrorReason &);
	string_view reflect(const rocksdb::RandomAccessFile::AccessPattern &p);
	const std::string &reflect(const rocksdb::Tickers &);
	const std::string &reflect(const rocksdb::Histograms &);

	// string_view <-> slice
	rocksdb::Slice slice(const string_view &);
	string_view slice(const rocksdb::Slice &);

	// Frequently used get options and set options are separate from the string/map system
	rocksdb::WriteOptions &operator+=(rocksdb::WriteOptions &, const sopts &);
	rocksdb::ReadOptions &operator+=(rocksdb::ReadOptions &, const gopts &);
	rocksdb::WriteOptions make_opts(const sopts &);
	rocksdb::ReadOptions make_opts(const gopts &);

	// Database options creator
	bool optstr_find_and_remove(std::string &optstr, const std::string &what);
	rocksdb::DBOptions make_dbopts(std::string optstr, std::string *const &out = nullptr, bool *read_only = nullptr, bool *fsck = nullptr);
	rocksdb::CompressionType find_supported_compression(const std::string &);

	// Validation functors
	bool valid(const rocksdb::Iterator &);
	bool operator!(const rocksdb::Iterator &);
	using valid_proffer = std::function<bool (const rocksdb::Iterator &)>;
	bool valid(const rocksdb::Iterator &, const valid_proffer &);
	bool valid_eq(const rocksdb::Iterator &, const string_view &);
	bool valid_lte(const rocksdb::Iterator &, const string_view &);
	bool valid_gt(const rocksdb::Iterator &, const string_view &);
	void valid_or_throw(const rocksdb::Iterator &);
	void valid_eq_or_throw(const rocksdb::Iterator &, const string_view &);

	// [GET] seek suite
	template<class pos> bool seek(database::column &, const pos &, const rocksdb::ReadOptions &, std::unique_ptr<rocksdb::Iterator> &it);
	template<class pos> bool seek(database::column &, const pos &, const gopts &, std::unique_ptr<rocksdb::Iterator> &it);
	std::unique_ptr<rocksdb::Iterator> seek(column &, const gopts &);
	std::unique_ptr<rocksdb::Iterator> seek(column &, const string_view &key, const gopts &);
	std::vector<row::value_type> seek(database &, const gopts &);
	std::pair<string_view, string_view> operator*(const rocksdb::Iterator &);

	// [SET] writebatch suite
	std::string debug(const rocksdb::WriteBatch &);
	bool has(const rocksdb::WriteBatch &, const op &);
	void commit(database &, rocksdb::WriteBatch &, const rocksdb::WriteOptions &);
	void commit(database &, rocksdb::WriteBatch &, const sopts &);
	void append(rocksdb::WriteBatch &, column &, const column::delta &delta);
	void append(rocksdb::WriteBatch &, const cell::delta &delta);

	std::vector<std::string> column_names(const std::string &path, const rocksdb::DBOptions &);
	std::vector<std::string> column_names(const std::string &path, const std::string &options);
}

struct ircd::db::throw_on_error
{
	throw_on_error(const rocksdb::Status & = rocksdb::Status::OK());
};

struct ircd::db::error_to_status
:rocksdb::Status
{
	error_to_status(const std::error_code &);
	error_to_status(const fs::error &);
	error_to_status(const std::exception &);
};
