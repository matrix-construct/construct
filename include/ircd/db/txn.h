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
#define HAVE_IRCD_DB_TXN_H

namespace ircd::db
{
	struct txn;

	bool for_each(const txn &, const std::function<bool (const delta &)> &);
	void for_each(const txn &, const std::function<void (const delta &)> &);
	std::string debug(const txn &);

	using seq_closure_bool = std::function<bool (txn &, const uint64_t &)>;
	using seq_closure = std::function<void (txn &, const uint64_t &)>;
	bool for_each(database &d, const uint64_t &seq, const seq_closure_bool &);
	void for_each(database &d, const uint64_t &seq, const seq_closure &);
	void get(database &d, const uint64_t &seq, const seq_closure &);
}

struct ircd::db::txn
{
	enum state :uint8_t;

	database *d {nullptr};
	std::unique_ptr<rocksdb::WriteBatch> wb;
	enum state state {0};

  public:
	struct opts;
	struct checkpoint;
	struct append;
	struct handler;

	using delta_closure = std::function<void (const delta &)>;
	using delta_closure_bool = std::function<bool (const delta &)>;
	using value_closure = std::function<void (const string_view &)>;

	explicit operator const rocksdb::WriteBatch &() const;
	explicit operator const database &() const;
	explicit operator rocksdb::WriteBatch &();
	explicit operator database &();

	bool get(const op &, const string_view &col, const string_view &key, const value_closure &) const;
	void at(const op &, const string_view &col, const string_view &key, const value_closure &) const;
	bool has(const op &, const string_view &col, const string_view &key) const;

	bool get(const op &, const string_view &col, const delta_closure &) const;
	void at(const op &, const string_view &col, const delta_closure &) const;
	bool has(const op &, const string_view &col) const;
	bool has(const op &) const;

	size_t bytes() const;   // size of data in txn.
	size_t size() const;    // count of updates in txn.

	// commit
	void operator()(database &, const sopts & = {});
	void operator()(const sopts & = {});

	// clear
	void clear();

	txn() = default;
	txn(database &);
	txn(database &, const opts &);
	txn(database &, std::unique_ptr<rocksdb::WriteBatch> &&);
	~txn() noexcept;
};

enum ircd::db::txn::state
:uint8_t
{
	BUILD       = 0,
	COMMIT      = 1,
	COMMITTED   = 2,
};

struct ircd::db::txn::append
{
	append(txn &, database &, const delta &);
	append(txn &, column &, const column::delta &);
	append(txn &, const cell::delta &);
	append(txn &, const row::delta &);
	append(txn &, const delta &);
	append(txn &, const string_view &key, const json::iov &);
};

struct ircd::db::txn::checkpoint
{
	txn &t;

	checkpoint(txn &);
	~checkpoint() noexcept;
};

struct ircd::db::txn::opts
{
	size_t reserve_bytes = 0;
	size_t max_bytes = 0;
};
