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
}

class ircd::db::txn
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
	template<class... T> append(txn &, const string_view &key, const json::tuple<T...> &);
	template<class... T> append(txn &, const string_view &key, const json::tuple<T...> &, std::array<column, sizeof...(T)> &);
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
                              const json::tuple<T...> &tuple)
{
	for_each(tuple, [&txn, &key](const auto &col, auto&& val)
	{
		if(defined(val)) append
		{
			txn, delta
			{
				col, key, byte_view<string_view>{val}
			}
		};
	});
}

template<class... T>
ircd::db::txn::append::append(txn &txn,
                              const string_view &key,
                              const json::tuple<T...> &tuple,
                              std::array<column, sizeof...(T)> &col)
{
	size_t i{0};
	for_each(tuple, [&txn, &key, &col, &i](const auto &, auto&& val)
	{
		if(defined(val)) append
		{
			txn, col.at(i), column::delta
			{
				key, byte_view<string_view>{val}
			}
		};

		++i;
	});
}
