/*
 * Copyright (C) 2017 Charybdis Development Team
 * Copyright (C) 2017 Jason Volk <jason@zemos.net>
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
#define HAVE_IRCD_DB_TXN_H

namespace ircd::db
{
	struct txn;

	void for_each(const txn &, const std::function<void (const delta &)> &);
	std::string debug(const txn &);
}

class ircd::db::txn
{
	database *d {nullptr};
	std::unique_ptr<rocksdb::WriteBatch> wb;

  public:
	struct opts;
	struct append;
	struct handler;
	struct checkpoint;

	explicit operator const rocksdb::WriteBatch &() const;
	explicit operator const database &() const;
	explicit operator rocksdb::WriteBatch &();
	explicit operator database &();

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

struct ircd::db::txn::checkpoint
{
	txn &t;

	checkpoint(txn &);
	~checkpoint() noexcept;
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

struct ircd::db::txn::opts
{
	size_t reserve_bytes = 0;
	size_t max_bytes = 0;
};
