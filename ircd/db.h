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

namespace ircd::db
{
	struct throw_on_error;

	const auto BLOCKING = rocksdb::ReadTier::kReadAllTier;
	const auto NON_BLOCKING = rocksdb::ReadTier::kBlockCacheTier;
	const auto DEFAULT_READAHEAD = 4_MiB;

	// Dedicated logging facility for rocksdb's log callbacks
	extern log::log rog;

	string_view reflect(const rocksdb::Env::Priority &p);
	string_view reflect(const rocksdb::Env::IOPriority &p);
	string_view reflect(const rocksdb::RandomAccessFile::AccessPattern &p);
	const std::string &reflect(const rocksdb::Tickers &);
	const std::string &reflect(const rocksdb::Histograms &);
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
