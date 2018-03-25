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

	bool test(const txn &, const std::function<bool (const delta &)> &);
	bool until(const txn &, const std::function<bool (const delta &)> &);
	void for_each(const txn &, const std::function<void (const delta &)> &);
	std::string debug(const txn &);

	using seq_closure_bool = std::function<bool (txn &, uint64_t)>;
	using seq_closure = std::function<void (txn &, uint64_t)>;
	bool for_each(database &d, const uint64_t &seq, const seq_closure_bool &);
	void for_each(database &d, const uint64_t &seq, const seq_closure &);
}

struct ircd::db::txn
{
	database *d {nullptr};
	std::unique_ptr<rocksdb::WriteBatch> wb;

  public:
	struct opts;
	struct checkpoint;
	struct append;
	struct handler;

	explicit operator const rocksdb::WriteBatch &() const;
	explicit operator const database &() const;
	explicit operator rocksdb::WriteBatch &();
	explicit operator database &();

	string_view get(const op &, const string_view &col, const string_view &key) const;
	string_view at(const op &, const string_view &col, const string_view &key) const;
	bool has(const op &, const string_view &col, const string_view &key) const;

	delta get(const op &, const string_view &col) const;
	delta at(const op &, const string_view &col) const;
	bool has(const op &, const string_view &col) const;
	bool has(const op &) const;

	size_t bytes() const;
	size_t size() const;

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

struct ircd::db::txn::append
{
	append(txn &, database &, const delta &);
	append(txn &, column &, const column::delta &);
	append(txn &, const cell::delta &);
	append(txn &, const row::delta &);
	append(txn &, const delta &);
	append(txn &, const string_view &key, const json::iov &);
	template<class... T> append(txn &, const string_view &key, const json::tuple<T...> &, const op & = op::SET);
	template<class... T> append(txn &, const string_view &key, const json::tuple<T...> &, std::array<column, sizeof...(T)> &, const op & = op::SET);
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

template<class... T>
ircd::db::txn::append::append(txn &txn,
                              const string_view &key,
                              const json::tuple<T...> &tuple,
                              const op &op)
{
	for_each(tuple, [&txn, &key, &op]
	(const auto &col, auto&& val)
	{
		if(!value_required(op) || defined(val)) append
		{
			txn, delta
			{
				op,
				col,
				key,
				value_required(op)?
					byte_view<string_view>{val}:
					byte_view<string_view>{}
			}
		};
	});
}

template<class... T>
ircd::db::txn::append::append(txn &txn,
                              const string_view &key,
                              const json::tuple<T...> &tuple,
                              std::array<column, sizeof...(T)> &col,
                              const op &op)
{
	size_t i{0};
	for_each(tuple, [&txn, &key, &col, &op, &i]
	(const auto &, auto&& val)
	{
		if(!value_required(op) || defined(val)) append
		{
			txn, col.at(i), column::delta
			{
				op,
				key,
				value_required(op)?
					byte_view<string_view>{val}:
					byte_view<string_view>{}
			}
		};

		++i;
	});
}
